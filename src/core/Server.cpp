#include "Server.h"

#include <cctype>
#include <numeric>

#include "Version.h"
#include "log/Logger.h"
#include "utils/Date.h"

using namespace vacdm;
using namespace vacdm::com;
using namespace vacdm::logging;

static std::string __receivedDeleteData;
static std::string __receivedGetData;
static std::string __receivedPatchData;
static std::string __receivedPostData;

static std::size_t receiveCurlDelete(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;

    std::string serverResult = static_cast<char*>(ptr);
    __receivedDeleteData += serverResult;
    return size * nmemb;
}

static std::size_t receiveCurlGet(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;

    std::string serverResult = static_cast<char*>(ptr);
    __receivedGetData += serverResult;
    return size * nmemb;
}

static std::size_t receiveCurlPatch(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;

    std::string serverResult = static_cast<char*>(ptr);
    __receivedPatchData += serverResult;
    return size * nmemb;
}

static std::size_t receiveCurlPost(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;

    std::string serverResult = static_cast<char*>(ptr);
    __receivedPostData += serverResult;
    return size * nmemb;
}

Server::Server()
    : m_apiKey(),
      m_getRequest(),
      m_postRequest(),
      m_patchRequest(),
      m_deleteRequest(),
      m_apiIsChecked(false),
      m_apiIsValid(false),
      m_backendOnline(false),
      m_baseUrl("https://app.vacdm.net"),
      m_masterAirports(),
      m_errorCode() {
    /* configure the get request */
    curl_easy_setopt(m_getRequest.socket, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_getRequest.socket, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_getRequest.socket, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
    curl_easy_setopt(m_getRequest.socket, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(m_getRequest.socket, CURLOPT_WRITEFUNCTION, receiveCurlGet);
    curl_easy_setopt(m_getRequest.socket, CURLOPT_TIMEOUT, 2L);

    /* configure the post request */
    curl_easy_setopt(m_postRequest.socket, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_postRequest.socket, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_postRequest.socket, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
    curl_easy_setopt(m_postRequest.socket, CURLOPT_WRITEFUNCTION, receiveCurlPost);
    curl_easy_setopt(m_postRequest.socket, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(m_postRequest.socket, CURLOPT_VERBOSE, 1);

    /* configure the patch request */
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_WRITEFUNCTION, receiveCurlPatch);
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_VERBOSE, 1);

    /* configure the delete request */
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_WRITEFUNCTION, receiveCurlDelete);
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_TIMEOUT, 2L);
}

void Server::setApiKey(const std::string& apiKey) {
    this->m_apiKey = apiKey;
    this->setCid(this->m_cid);
}

void Server::setCid(const std::string& cid) {
    this->m_cid = cid;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, ("x-api-key: " + this->m_apiKey).c_str());
    if (!this->m_cid.empty()) {
        headers = curl_slist_append(headers, ("x-vatsim-cid: " + this->m_cid).c_str());
    }
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(m_getRequest.socket, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_postRequest.socket, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_patchRequest.socket, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_deleteRequest.socket, CURLOPT_HTTPHEADER, headers);
}

Server::~Server() {
    if (nullptr != m_getRequest.socket) {
        std::lock_guard guard(m_getRequest.lock);
        curl_easy_cleanup(m_getRequest.socket);
        m_getRequest.socket = nullptr;
    }

    if (nullptr != m_postRequest.socket) {
        std::lock_guard guard(m_postRequest.lock);
        curl_easy_cleanup(m_postRequest.socket);
        m_postRequest.socket = nullptr;
    }

    if (nullptr != m_patchRequest.socket) {
        std::lock_guard guard(m_patchRequest.lock);
        curl_easy_cleanup(m_patchRequest.socket);
        m_patchRequest.socket = nullptr;
    }

    if (nullptr != m_deleteRequest.socket) {
        std::lock_guard guard(m_deleteRequest.lock);
        curl_easy_cleanup(m_deleteRequest.socket);
        m_deleteRequest.socket = nullptr;
    }
}

void Server::changeServerAddress(const std::string& url) {
    this->m_baseUrl = url;
    this->m_apiIsChecked = false;
    this->m_apiIsValid = false;
}

bool Server::checkWebApi() {
    if (this->m_apiIsChecked == true) return this->m_apiIsValid;

    std::lock_guard guard(m_getRequest.lock);
    if (m_getRequest.socket == nullptr) {
        this->m_apiIsValid = false;
        return m_apiIsValid;
    }

    __receivedGetData.clear();

    std::string url = m_baseUrl + "/api/v1/health";
    curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());

    // send the GET request
    CURLcode result = curl_easy_perform(m_getRequest.socket);
    if (result != CURLE_OK) {
        this->m_apiIsValid = false;
        return m_apiIsValid;
    }

    Json::CharReaderBuilder builder{};
    auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    std::string errors;
    Json::Value root;
    Logger::instance().log(Logger::LogSender::Server, "Received backend health response: " + __receivedGetData,
                           Logger::LogLevel::Info);
    if (reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(), &root,
                      &errors)) {
        if (root.get("status", Json::Value("")).asString() != "ok") {
            this->m_errorCode = "Backend health endpoint returned invalid status";
            this->m_apiIsValid = false;
            this->m_backendOnline = false;
        } else {
            this->m_apiIsValid = true;
            this->m_backendOnline = true;
        }

    } else {
        this->m_errorCode = "Invalid backend-health response: " + __receivedGetData;
        this->m_apiIsValid = false;
        this->m_backendOnline = false;
    }
    m_apiIsChecked = true;
    return this->m_apiIsValid;
}

bool Server::backendOnline() const { return this->m_backendOnline; }

Server::ServerConfiguration Server::getServerConfig() {
    if (false == this->m_apiIsChecked || false == this->m_apiIsValid) return Server::ServerConfiguration();

    std::lock_guard guard(m_getRequest.lock);
    if (nullptr != m_getRequest.socket) {
        __receivedGetData.clear();

        std::string url = m_baseUrl + "/api/v1/config";
        curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());

        /* send the command */
        CURLcode result = curl_easy_perform(m_getRequest.socket);
        if (CURLE_OK == result) {
            Json::CharReaderBuilder builder{};
            auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
            std::string errors;
            Json::Value root;

            Logger::instance().log(Logger::LogSender::Server, "Received configuration: " + __receivedGetData,
                                   Logger::LogLevel::Info);
            if (reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(), &root,
                              &errors)) {
                ServerConfiguration_t config;
                config.name = root["serverName"].asString();
                config.allowMasterInSweatbox = root["allowSimSession"].asBool();
                config.allowMasterAsObserver = root["allowObsMaster"].asBool();
                return config;
            }
        }
    }

    return ServerConfiguration();
}

std::list<types::Pilot> Server::getPilots(const std::list<std::string> airports) {
    std::lock_guard guard(m_getRequest.lock);
    if (nullptr != m_getRequest.socket) {
        __receivedGetData.clear();

        std::string url = m_baseUrl + "/api/v1/pilots";
        if (airports.size() != 0) {
            url +=
                "?airport=" +
                std::accumulate(std::next(airports.begin()), airports.end(), airports.front(),
                                [](const std::string& acc, const std::string& str) { return acc + "&airport=" + str; });
        }
        Logger::instance().log(Logger::LogSender::Server, url, Logger::LogLevel::Info);

        curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());

        // send GET request
        CURLcode result = curl_easy_perform(m_getRequest.socket);
        if (result == CURLE_OK) {
            Json::CharReaderBuilder builder{};
            auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
            std::string errors;
            Json::Value root;

            // Logger::instance().log(Logger::LogSender::Server, "Received data" + __receivedGetData,
            //                        Logger::LogLevel::Debug);
            if (reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(), &root,
                              &errors) &&
                root.isArray()) {
                std::list<types::Pilot> pilots;

                for (const auto& pilot : std::as_const(root)) {
                    pilots.push_back(types::Pilot());

                    pilots.back().callsign = pilot["callsign"].asString();
                    pilots.back().cid = pilot.get("cid", Json::Value("")).asString();
                    pilots.back().lastUpdate = utils::Date::isoStringToTimestamp(pilot["updatedAt"].asString());
                    pilots.back().inactive = pilot["inactive"].asBool();

                    // position data
                    pilots.back().latitude = pilot["position"]["lat"].asDouble();
                    pilots.back().longitude = pilot["position"]["lon"].asDouble();
                    pilots.back().taxizoneIsTaxiout = pilot["vacdm"]["taxizoneIsTaxiout"].asBool();

                    // flightplan & clearance data
                    pilots.back().origin = pilot.isMember("adep") ? pilot["adep"].asString()
                                                                   : pilot["flightplan"]["departure"].asString();
                    pilots.back().destination = pilot.isMember("ades") ? pilot["ades"].asString()
                                                                        : pilot["flightplan"]["arrival"].asString();
                    pilots.back().runway = pilot.isMember("runway") ? pilot["runway"].asString()
                                                                     : pilot["clearance"]["dep_rwy"].asString();
                    pilots.back().sid = pilot["clearance"]["sid"].asString();
                    pilots.back().aircraft = pilot.get("aircraft", Json::Value("")).asString();
                    pilots.back().flightType = pilot.get("flightType", Json::Value("")).asString();
                    pilots.back().airline = pilot.get("airline", Json::Value("")).asString();
                    pilots.back().exemptFromCdm = pilot.get("exemptFromCdm", Json::Value(false)).asBool();

                    // ACDM procedure data
                    const Json::Value vacdm = pilot.isMember("vacdm") ? pilot["vacdm"] : Json::Value();
                    const auto fieldOrLegacy = [&pilot, &vacdm](const char* field) -> Json::Value {
                        if (pilot.isMember(field)) return pilot[field];
                        if (vacdm.isObject() && vacdm.isMember(field)) return vacdm[field];
                        return Json::Value();
                    };
                    pilots.back().eobt = utils::Date::isoStringToTimestamp(fieldOrLegacy("eobt").asString());
                    pilots.back().tobt = utils::Date::isoStringToTimestamp(fieldOrLegacy("tobt").asString());
                    pilots.back().tobt_state = fieldOrLegacy("tobt_state").asString();
                    pilots.back().ctot = utils::Date::isoStringToTimestamp(fieldOrLegacy("ctot").asString());
                    pilots.back().ttot = utils::Date::isoStringToTimestamp(fieldOrLegacy("ttot").asString());
                    pilots.back().tsat = utils::Date::isoStringToTimestamp(fieldOrLegacy("tsat").asString());
                    pilots.back().exot =
                        std::chrono::utc_clock::time_point(std::chrono::minutes(fieldOrLegacy("exot").asInt64()));
                    pilots.back().asat = utils::Date::isoStringToTimestamp(fieldOrLegacy("asat").asString());
                    pilots.back().aobt = utils::Date::isoStringToTimestamp(fieldOrLegacy("aobt").asString());
                    pilots.back().atot = utils::Date::isoStringToTimestamp(fieldOrLegacy("atot").asString());
                    pilots.back().asrt = utils::Date::isoStringToTimestamp(fieldOrLegacy("asrt").asString());
                    pilots.back().aort = utils::Date::isoStringToTimestamp(fieldOrLegacy("aort").asString());
                    
                    // Phase 1+ fields
                    pilots.back().tsac = pilot.get("tsac", Json::Value("")).asString();
                    pilots.back().tobtSetBy = pilot.get("tobtSetBy", Json::Value("")).asString();
                    pilots.back().tsatReset = pilot.get("tsatReset", Json::Value(false)).asBool();


                    // event booking data
                    pilots.back().hasBooking = pilot["hasBooking"].asBool();
                }
                Logger::instance().log(Logger::LogSender::Server, "Pilots size: " + std::to_string(pilots.size()),
                                       Logger::LogLevel::Info);
                return pilots;
            } else {
                Logger::instance().log(Logger::LogSender::Server, "Error " + errors, Logger::LogLevel::Info);
            }
        }
    }

    return {};
}

void Server::sendPostMessage(const std::string& endpointUrl, const Json::Value& root) {
    if (this->m_apiIsChecked == false || this->m_apiIsValid == false || this->m_masterAirports.empty()) return;

    Json::StreamWriterBuilder builder{};
    const auto message = Json::writeString(builder, root);

    Logger::instance().log(Logger::LogSender::Server,
                           "Posting " + root["callsign"].asString() + " with message: " + message,
                           Logger::LogLevel::Debug);

    std::lock_guard guard(this->m_postRequest.lock);
    if (m_postRequest.socket != nullptr) {
        std::string url = m_baseUrl + endpointUrl;
        curl_easy_setopt(m_postRequest.socket, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_postRequest.socket, CURLOPT_POSTFIELDS, message.c_str());

        curl_easy_perform(m_postRequest.socket);

        Logger::instance().log(Logger::LogSender::Server,
                               "Posted " + root["callsign"].asString() + " response: " + __receivedPostData,
                               Logger::LogLevel::Debug);
        __receivedPostData.clear();
    }
}

void Server::sendPatchMessage(const std::string& endpointUrl, const Json::Value& root) {
    if (this->m_apiIsChecked == false || this->m_apiIsValid == false || this->m_masterAirports.empty()) return;

    Json::StreamWriterBuilder builder{};
    const auto message = Json::writeString(builder, root);

    Logger::instance().log(Logger::LogSender::Server,
                           "Patching " + root["callsign"].asString() + " with message: " + message,
                           Logger::LogLevel::Debug);

    std::lock_guard guard(this->m_patchRequest.lock);
    if (m_patchRequest.socket != nullptr) {
        std::string url = m_baseUrl + endpointUrl;
        curl_easy_setopt(m_patchRequest.socket, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_patchRequest.socket, CURLOPT_POSTFIELDS, message.c_str());

        curl_easy_perform(m_patchRequest.socket);

        Logger::instance().log(Logger::LogSender::Server,
                               "Patched " + root["callsign"].asString() + " response: " + __receivedPatchData,
                               Logger::LogLevel::Debug);
        __receivedPatchData.clear();
    }
}

void Server::sendDeleteMessage(const std::string& endpointUrl) {
    if (this->m_apiIsChecked == false || this->m_apiIsValid == false || this->m_masterAirports.empty()) return;

    Json::StreamWriterBuilder builder{};

    std::lock_guard guard(this->m_deleteRequest.lock);
    if (m_deleteRequest.socket != nullptr) {
        std::string url = m_baseUrl + endpointUrl;

        curl_easy_setopt(m_deleteRequest.socket, CURLOPT_URL, url.c_str());

        curl_easy_perform(m_deleteRequest.socket);
        __receivedDeleteData.clear();
    }
}

void Server::postPilot(types::Pilot pilot) {
    Json::Value root;

    root["callsign"] = pilot.callsign;
    root["cid"] = pilot.cid;
    root["adep"] = pilot.origin;
    root["ades"] = pilot.destination;
    root["eobt"] = utils::Date::timestampToIsoString(pilot.eobt);
    root["runway"] = pilot.runway;
    root["taxizone"] = Json::Value::nullSingleton();
    root["aircraft"] = pilot.aircraft;
    const bool isDomestic = pilot.origin.rfind("VV", 0) == 0 && pilot.destination.rfind("VV", 0) == 0;
    root["flightType"] = pilot.flightType.empty() ? (isDomestic ? "DOMESTIC" : "INTERNATIONAL") : pilot.flightType;
    root["airline"] = pilot.airline;
    root["exemptFromCdm"] = false;

    this->sendPostMessage("/api/v1/pilots", root);
}

bool Server::isReadOnlyAirport(const std::string& icao) {
    if (icao.empty()) return false;
    std::lock_guard guard(m_getRequest.lock);
    if (m_getRequest.socket == nullptr) return false;

    __receivedGetData.clear();
    std::string url = m_baseUrl + "/api/v1/airports/" + icao;
    curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());
    CURLcode result = curl_easy_perform(m_getRequest.socket);
    if (result != CURLE_OK) return false;

    Json::CharReaderBuilder builder{};
    auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    std::string errors;
    Json::Value root;
    if (!reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(), &root,
                       &errors)) {
        return false;
    }

    const std::string status = root.get("acdmStatus", Json::Value("FULL")).asString();
    return status == "PRE_CDM" || status == "INACTIVE";
}

void Server::updateExot(const std::string& callsign, const std::chrono::utc_clock::time_point& exot) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["exot"] = std::chrono::duration_cast<std::chrono::minutes>(exot.time_since_epoch()).count();
    root["vacdm"]["tsat"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["ttot"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["asat"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["aobt"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["atot"] = utils::Date::timestampToIsoString(types::defaultTime);

    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::updateTobt(const types::Pilot& pilot, const std::chrono::utc_clock::time_point& tobt, bool manualTobt) {
    Json::Value root;

    bool resetTsat = (tobt == types::defaultTime && true == manualTobt) || tobt >= pilot.tsat;
    root["callsign"] = pilot.callsign;
    root["vacdm"] = Json::Value();

    root["vacdm"] = Json::Value();
    root["vacdm"]["tobt"] = utils::Date::timestampToIsoString(tobt);
    if (true == resetTsat) root["vacdm"]["tsat"] = utils::Date::timestampToIsoString(types::defaultTime);
    if (false == manualTobt) root["vacdm"]["tobt_state"] = "CONFIRMED";

    root["vacdm"]["ttot"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["asat"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["aobt"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["atot"] = utils::Date::timestampToIsoString(types::defaultTime);

    this->sendPatchMessage("/api/v1/pilots/" + pilot.callsign, root);
}

void Server::updateAsat(const std::string& callsign, const std::chrono::utc_clock::time_point& asat) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["asat"] = utils::Date::timestampToIsoString(asat);

    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::updateAsrt(const std::string& callsign, const std::chrono::utc_clock::time_point& asrt) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["asrt"] = utils::Date::timestampToIsoString(asrt);

    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::updateAobt(const std::string& callsign, const std::chrono::utc_clock::time_point& aobt) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["aobt"] = utils::Date::timestampToIsoString(aobt);

    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::updateAort(const std::string& callsign, const std::chrono::utc_clock::time_point& aort) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["aort"] = utils::Date::timestampToIsoString(aort);

    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::updateTsac(const std::string& callsign, const std::chrono::utc_clock::time_point& tsac) {
    Json::Value root;
    root["callsign"] = callsign;
    if (tsac == types::defaultTime) {
        root["tsac"] = Json::Value::nullSingleton();
    } else {
        // format as HHMM
        char buf[10];
        std::snprintf(buf, sizeof(buf), "%02d%02d", 
                      (int)std::chrono::duration_cast<std::chrono::hours>(tsac.time_since_epoch() % std::chrono::hours(24)).count(),
                      (int)std::chrono::duration_cast<std::chrono::minutes>(tsac.time_since_epoch() % std::chrono::hours(1)).count());
        root["tsac"] = buf;
    }
    this->sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::toggleLvo(const std::string& icao, bool active) {
    Json::Value root;
    root["active"] = active;
    this->sendPostMessage("/api/v1/airports/" + icao + "/lvo", root);
}

void Server::postDelay(const std::string& icao, const std::string& runway, const std::string& type, const std::string& time) {
    Json::Value root;
    root["airport"] = icao;
    root["runway"] = runway;
    root["type"] = type;
    root["fromTime"] = time;
    this->sendPostMessage("/api/v1/airports/" + icao + "/delays", root);
}

void Server::resetTobt(const std::string& callsign, const std::chrono::utc_clock::time_point& tobt,
                       const std::string& tobtState) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["tobt"] = utils::Date::timestampToIsoString(tobt);
    root["vacdm"]["tobt_state"] = tobtState;
    root["vacdm"]["tsat"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["ttot"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["asat"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["asrt"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["aobt"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["atot"] = utils::Date::timestampToIsoString(types::defaultTime);
    root["vacdm"]["aort"] = utils::Date::timestampToIsoString(types::defaultTime);

    sendPatchMessage("/api/v1/pilots/" + callsign, root);
}

void Server::deletePilot(const std::string& callsign) { sendDeleteMessage("/api/v1/pilots/" + callsign); }

void Server::claimMaster(const std::string& icao, const std::string& cid, const std::string& name) {
    Json::Value root;
    root["cid"] = cid;
    root["name"] = name;
    
    std::lock_guard guard(this->m_postRequest.lock);
    if (m_postRequest.socket != nullptr) {
        std::string url = m_baseUrl + "/api/v1/airports/" + icao + "/master";
        curl_easy_setopt(m_postRequest.socket, CURLOPT_URL, url.c_str());
        
        Json::StreamWriterBuilder builder{};
        std::string message = Json::writeString(builder, root);
        curl_easy_setopt(m_postRequest.socket, CURLOPT_POSTFIELDS, message.c_str());
        
        __receivedPostData.clear();
        CURLcode result = curl_easy_perform(m_postRequest.socket);
        
        if (result == CURLE_OK) {
            Json::CharReaderBuilder builder{};
            auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
            std::string errors;
            Json::Value resp;
            if (reader->parse(__receivedPostData.c_str(), __receivedPostData.c_str() + __receivedPostData.length(), &resp, &errors)) {
                if (resp.isMember("statusCode") && resp["statusCode"].asInt() == 409) {
                    m_errorCode = "Master claim rejected: " + resp["message"].asString();
                    return;
                }
                std::lock_guard lock(m_stateLock);
                m_masterAirports.insert(icao);
            }
        }
        __receivedPostData.clear();
    }
}

void Server::releaseMaster(const std::string& icao, const std::string& cid) {
    Json::Value root;
    root["cid"] = cid;
    
    std::lock_guard guard(this->m_deleteRequest.lock);
    if (m_deleteRequest.socket != nullptr) {
        std::string url = m_baseUrl + "/api/v1/airports/" + icao + "/master";
        curl_easy_setopt(m_deleteRequest.socket, CURLOPT_URL, url.c_str());
        
        // curl DELETE with body
        Json::StreamWriterBuilder builder{};
        std::string message = Json::writeString(builder, root);
        curl_easy_setopt(m_deleteRequest.socket, CURLOPT_POSTFIELDS, message.c_str());
        
        __receivedDeleteData.clear();
        curl_easy_perform(m_deleteRequest.socket);
        __receivedDeleteData.clear();
        
        std::lock_guard lock(m_stateLock);
        m_masterAirports.erase(icao);
    }
}

bool Server::isMaster(const std::string& icao) {
    std::lock_guard lock(m_stateLock);
    return m_masterAirports.find(icao) != m_masterAirports.end();
}

void Server::sendHeartbeats(const std::string& cid) {
    std::lock_guard lock(m_stateLock);
    if (m_masterAirports.empty()) return;
    
    for (const auto& icao : m_masterAirports) {
        Json::Value root;
        root["cid"] = cid;
        
        // We use patch request socket for PUT heartbeat
        std::lock_guard guard(this->m_patchRequest.lock);
        if (m_patchRequest.socket != nullptr) {
            std::string url = m_baseUrl + "/api/v1/airports/" + icao + "/master/heartbeat";
            curl_easy_setopt(m_patchRequest.socket, CURLOPT_URL, url.c_str());
            curl_easy_setopt(m_patchRequest.socket, CURLOPT_CUSTOMREQUEST, "PUT");
            
            Json::StreamWriterBuilder builder{};
            std::string message = Json::writeString(builder, root);
            curl_easy_setopt(m_patchRequest.socket, CURLOPT_POSTFIELDS, message.c_str());
            
            curl_easy_perform(m_patchRequest.socket);
            curl_easy_setopt(m_patchRequest.socket, CURLOPT_CUSTOMREQUEST, "PATCH"); // restore
        }
    }
}

std::set<std::string> Server::getMasterAirports() {
    std::lock_guard lock(m_stateLock);
    return m_masterAirports;
}

const std::string& Server::errorMessage() const { return this->m_errorCode; }

Server& Server::instance() {
    static Server __instance;
    return __instance;
}

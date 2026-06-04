#include "Server.h"

#include <algorithm>
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
static const std::set<std::string> kHardcodedSupportedAirports{"VVTS", "VVNB"};

static vacdm::types::Pilot parsePilotJson(const Json::Value& pilot) {
    vacdm::types::Pilot parsed;

    parsed.callsign = pilot["callsign"].asString();
    parsed.cid = pilot.get("cid", Json::Value("")).asString();
    parsed.lastUpdate = vacdm::utils::Date::isoStringToTimestamp(pilot["updatedAt"].asString());
    parsed.inactive = pilot["inactive"].asBool();

    parsed.latitude = pilot["position"]["lat"].asDouble();
    parsed.longitude = pilot["position"]["lon"].asDouble();
    parsed.taxizoneIsTaxiout = pilot["vacdm"]["taxizoneIsTaxiout"].asBool();

    parsed.origin = pilot.isMember("adep") ? pilot["adep"].asString() : pilot["flightplan"]["departure"].asString();
    parsed.destination = pilot.isMember("ades") ? pilot["ades"].asString() : pilot["flightplan"]["arrival"].asString();
    parsed.runway = pilot.isMember("runway") ? pilot["runway"].asString() : pilot["clearance"]["dep_rwy"].asString();
    parsed.sid = pilot.isMember("sid") ? pilot["sid"].asString() : pilot["clearance"]["sid"].asString();
    parsed.aircraft = pilot.get("aircraft", Json::Value("")).asString();
    parsed.route = pilot.get("route", Json::Value("")).asString();
    parsed.flightType = pilot.get("flightType", Json::Value("")).asString();
    parsed.airline = pilot.get("airline", Json::Value("")).asString();
    parsed.exemptFromCdm = pilot.get("exemptFromCdm", Json::Value(false)).asBool();

    const Json::Value vacdmJson = pilot.isMember("vacdm") ? pilot["vacdm"] : Json::Value();
    const auto fieldOrLegacy = [&pilot, &vacdmJson](const char* field) -> Json::Value {
        if (pilot.isMember(field)) return pilot[field];
        if (vacdmJson.isObject() && vacdmJson.isMember(field)) return vacdmJson[field];
        return Json::Value();
    };
    parsed.eobt = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("eobt").asString());
    parsed.tobt = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("tobt").asString());
    parsed.tobt_state = fieldOrLegacy("tobt_state").asString();
    parsed.ctot = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("ctot").asString());
    parsed.ttot = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("ttot").asString());
    parsed.tsat = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("tsat").asString());
    parsed.exot = std::chrono::utc_clock::time_point(std::chrono::minutes(fieldOrLegacy("exot").asInt64()));
    parsed.asat = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("asat").asString());
    parsed.aobt = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("aobt").asString());
    parsed.atot = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("atot").asString());
    parsed.asrt = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("asrt").asString());
    parsed.ardt = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("ardt").asString());
    parsed.aort = vacdm::utils::Date::isoStringToTimestamp(fieldOrLegacy("aort").asString());
    parsed.groundState = fieldOrLegacy("ground_state").asString();

    parsed.tsac = pilot.get("tsac", Json::Value("")).asString();
    parsed.tobtSetBy = pilot.get("tobtSetBy", Json::Value("")).asString();
    parsed.tsatReset = pilot.get("tsatReset", Json::Value(false)).asBool();
    parsed.hasBooking = pilot["hasBooking"].asBool();
    parsed.ctotStatus = pilot.get("eventCtotStatus", vacdmJson.get("ctotStatus", Json::Value(""))).asString();
    parsed.ready = vacdmJson.get("ready", Json::Value(false)).asBool();

    return parsed;
}

static std::size_t receiveCurlDelete(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;
    __receivedDeleteData.append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static std::size_t receiveCurlGet(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;
    __receivedGetData.append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static std::size_t receiveCurlPatch(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;
    __receivedPatchData.append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static std::size_t receiveCurlPost(void* ptr, std::size_t size, std::size_t nmemb, void* stream) {
    (void)stream;
    __receivedPostData.append(static_cast<char*>(ptr), size * nmemb);
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
      m_lastPilotFetchOk(false),
      m_baseUrl("https://api.vclvacc.net"),
      m_masterAirports(),
      m_supportedAirports(kHardcodedSupportedAirports),
      m_airportMetadata(),
      m_pilotSyncRevision(),
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
        const bool isNumericCid = std::all_of(this->m_cid.begin(), this->m_cid.end(),
                                              [](unsigned char c) { return std::isdigit(c) != 0; });
        headers = curl_slist_append(
            headers,
            ((isNumericCid ? "x-vatsim-cid: " : "x-vacdm-position: ") + this->m_cid).c_str());
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

bool Server::lastPilotFetchOk() const { return this->m_lastPilotFetchOk; }

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
    this->m_lastPilotFetchOk = false;

    std::list<std::string> supportedAirports;
    for (const auto& icao : airports) {
        if (this->isSupportedAirport(icao)) supportedAirports.push_back(icao);
    }

    std::lock_guard guard(m_getRequest.lock);
    if (nullptr != m_getRequest.socket) {
        __receivedGetData.clear();

        std::string url = m_baseUrl + "/api/v1/pilots";
        if (supportedAirports.size() != 0) {
            url +=
                "?airport=" +
                std::accumulate(std::next(supportedAirports.begin()), supportedAirports.end(), supportedAirports.front(),
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
                this->m_lastPilotFetchOk = true;
                std::list<types::Pilot> pilots;

                for (const auto& pilot : std::as_const(root)) {
                    pilots.push_back(parsePilotJson(pilot));
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

Server::PilotSyncResult Server::getPilotSync(const std::list<std::string> airports) {
    this->m_lastPilotFetchOk = false;

    std::list<std::string> supportedAirports;
    for (const auto& icao : airports) {
        if (this->isSupportedAirport(icao)) supportedAirports.push_back(icao);
    }

    std::string revision;
    {
        std::lock_guard lock(m_stateLock);
        revision = this->m_pilotSyncRevision;
    }

    {
        std::lock_guard guard(m_getRequest.lock);
        if (nullptr != m_getRequest.socket) {
            __receivedGetData.clear();

            std::string url = m_baseUrl + "/api/v1/pilots/sync";
            bool hasQuery = false;
            for (const auto& icao : supportedAirports) {
                url += hasQuery ? "&airport=" : "?airport=";
                url += icao;
                hasQuery = true;
            }
            if (!revision.empty()) {
                char* escaped = curl_easy_escape(m_getRequest.socket, revision.c_str(), 0);
                if (escaped != nullptr) {
                    url += hasQuery ? "&since=" : "?since=";
                    url += escaped;
                    curl_free(escaped);
                    hasQuery = true;
                }
            }

            Logger::instance().log(Logger::LogSender::Server, url, Logger::LogLevel::Info);
            curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());

            CURLcode result = curl_easy_perform(m_getRequest.socket);
            if (result == CURLE_OK) {
                Json::CharReaderBuilder builder{};
                auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
                std::string errors;
                Json::Value root;

                if (reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(),
                                  &root, &errors) &&
                    root.isObject() && root["pilots"].isArray() && root["deleted"].isArray()) {
                    PilotSyncResult sync;
                    sync.ok = true;
                    sync.full = root.get("full", Json::Value(true)).asBool();
                    sync.revision = root.get("revision", Json::Value("")).asString();

                    for (const auto& pilot : std::as_const(root["pilots"])) {
                        sync.pilots.push_back(parsePilotJson(pilot));
                    }
                    for (const auto& callsign : std::as_const(root["deleted"])) {
                        sync.deleted.insert(callsign.asString());
                    }

                    this->m_lastPilotFetchOk = true;
                    if (!sync.revision.empty()) {
                        std::lock_guard lock(m_stateLock);
                        this->m_pilotSyncRevision = sync.revision;
                    }

                    Logger::instance().log(
                        Logger::LogSender::Server,
                        "Pilot sync: full=" + std::string(sync.full ? "true" : "false") +
                            " pilots=" + std::to_string(sync.pilots.size()) +
                            " deleted=" + std::to_string(sync.deleted.size()),
                        Logger::LogLevel::Info);
                    return sync;
                }

                Logger::instance().log(Logger::LogSender::Server,
                                       "Pilot sync parse failed, falling back to full polling: " + errors,
                                       Logger::LogLevel::Info);
            }
        }
    }

    PilotSyncResult fallback;
    fallback.pilots = this->getPilots(airports);
    fallback.ok = this->lastPilotFetchOk();
    fallback.full = true;
    return fallback;
}

void Server::resetPilotSyncRevision() {
    std::lock_guard lock(m_stateLock);
    this->m_pilotSyncRevision.clear();
}

void Server::sendPostMessage(const std::string& endpointUrl, const Json::Value& root) {
    if (this->m_apiIsChecked == false || this->m_apiIsValid == false || this->m_masterAirports.empty()) return;

    Json::StreamWriterBuilder builder{};
    const auto message = Json::writeString(builder, root);

    std::string logId = root.get("callsign", "System").asString();

    Logger::instance().log(Logger::LogSender::Server,
                           "Posting " + logId + " with message: " + message,
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

    std::string logId = root.get("callsign", "System").asString();

    Logger::instance().log(Logger::LogSender::Server,
                           "Patching " + logId + " with message: " + message,
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
    root["route"] = pilot.route;
    const bool isDomestic = pilot.origin.rfind("VV", 0) == 0 && pilot.destination.rfind("VV", 0) == 0;
    root["flightType"] = pilot.flightType.empty() ? (isDomestic ? "DOMESTIC" : "INTERNATIONAL") : pilot.flightType;
    root["airline"] = pilot.airline;
    root["exemptFromCdm"] = false;
    root["vacdm"] = Json::Value();
    root["vacdm"]["ground_state"] = pilot.groundState;
    if (pilot.forceReactivate) root["vacdm"]["forceReactivate"] = true;

    this->sendPostMessage("/api/v1/pilots", root);
}

void Server::refreshAirportMetadata(const std::string& icao) {
    if (icao.empty() || !this->isSupportedAirport(icao)) return;
    std::lock_guard guard(m_getRequest.lock);
    if (m_getRequest.socket == nullptr) return;

    __receivedGetData.clear();
    std::string url = m_baseUrl + "/api/v1/airports/" + icao;
    curl_easy_setopt(m_getRequest.socket, CURLOPT_URL, url.c_str());
    CURLcode result = curl_easy_perform(m_getRequest.socket);
    if (result != CURLE_OK) return;

    Json::CharReaderBuilder builder{};
    auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    std::string errors;
    Json::Value root;
    if (reader->parse(__receivedGetData.c_str(), __receivedGetData.c_str() + __receivedGetData.length(), &root,
                       &errors)) {
        AirportMetadata meta;
        meta.icao = icao;
        meta.status = root.get("acdmStatus", Json::Value("FULL")).asString();
        meta.readOnly = (meta.status == "PRE_CDM" || meta.status == "INACTIVE");
        meta.master = root.get("master", Json::Value("")).asString();
        meta.lvo = root.get("lvoActive", root.get("lvo", Json::Value(false))).asBool();

        if (root.isMember("delays") && root["delays"].isArray()) {
            for (const auto& delay : root["delays"]) {
                std::string rwy = delay.get("runway", "").asString();
                std::string type = delay.get("type", "").asString();
                std::string time = delay.get("fromTime", "").asString();
                if (!rwy.empty()) {
                    meta.activeDelays.push_back(rwy + " " + type + " " + time + "z");
                }
            }
        }

        std::lock_guard lock(m_stateLock);
        m_airportMetadata[icao] = meta;
    }
}

void Server::refreshSupportedAirports() {
    std::lock_guard lock(m_stateLock);
    m_supportedAirports = kHardcodedSupportedAirports;
    Logger::instance().log(Logger::LogSender::Server, "Supported airports hardcoded: VVTS VVNB",
                           Logger::LogLevel::Info);
}

Server::AirportMetadata Server::getAirportMetadata(const std::string& icao) {
    std::lock_guard lock(m_stateLock);
    if (m_airportMetadata.find(icao) != m_airportMetadata.end()) {
        return m_airportMetadata[icao];
    }
    AirportMetadata meta;
    meta.icao = icao;
    return meta;
}

bool Server::isReadOnlyAirport(const std::string& icao) {
    return this->getAirportMetadata(icao).readOnly;
}

bool Server::isSupportedAirport(const std::string& icao) {
    std::lock_guard lock(m_stateLock);
    return m_supportedAirports.find(icao) != m_supportedAirports.end();
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

void Server::updateArdt(const std::string& callsign, const std::chrono::utc_clock::time_point& ardt) {
    Json::Value root;

    root["callsign"] = callsign;
    root["vacdm"] = Json::Value();
    root["vacdm"]["ardt"] = utils::Date::timestampToIsoString(ardt);

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

void Server::probeParkingStand(const std::string& callsign, double lat, double lon) {
    Json::Value root;
    root["lat"] = lat;
    root["lon"] = lon;
    this->sendPatchMessage("/api/v1/pilots/" + callsign + "/probe-parking", root);
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
    if (!this->isSupportedAirport(icao)) {
        m_errorCode = "Master claim rejected: " + icao + " is not supported by this VCLvACDM deployment.";
        return;
    }

    this->setCid(cid);

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
        
        long responseCode = 0;
        curl_easy_getinfo(m_postRequest.socket, CURLINFO_RESPONSE_CODE, &responseCode);

        if (result == CURLE_OK) {
            if (responseCode == 409) {
                m_errorCode = "Master claim rejected: " + icao + " is already managed by another controller.";
                // Try to extract the name if present in JSON
                Json::CharReaderBuilder readerBuilder{};
                auto reader = std::unique_ptr<Json::CharReader>(readerBuilder.newCharReader());
                std::string errors;
                Json::Value resp;
                if (reader->parse(__receivedPostData.c_str(), __receivedPostData.c_str() + __receivedPostData.length(), &resp, &errors)) {
                    if (resp.isMember("message")) m_errorCode = "Master claim rejected: " + resp["message"].asString();
                }
                __receivedPostData.clear();
                return;
            } else if (responseCode >= 200 && responseCode < 300) {
                std::lock_guard lock(m_stateLock);
                m_masterAirports.insert(icao);
                m_errorCode = "";
            } else {
                m_errorCode = "Master claim failed for " + icao + " (HTTP " + std::to_string(responseCode) + ")";
            }
        } else {
            m_errorCode = "Master claim network error for " + icao;
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
        CURLcode result = curl_easy_perform(m_deleteRequest.socket);
        
        long responseCode = 0;
        curl_easy_getinfo(m_deleteRequest.socket, CURLINFO_RESPONSE_CODE, &responseCode);

        if (result == CURLE_OK && responseCode >= 200 && responseCode < 300) {
            std::lock_guard lock(m_stateLock);
            m_masterAirports.erase(icao);
            m_errorCode = "";
        } else {
            m_errorCode = "Master release failed for " + icao;
        }
        __receivedDeleteData.clear();
    }
}

void Server::releaseAllMasters(const std::string& cid) {
    std::set<std::string> masters;
    std::string releaseCid = cid;

    {
        std::lock_guard lock(m_stateLock);
        masters = m_masterAirports;
        if (releaseCid.empty()) releaseCid = m_cid;
    }

    if (releaseCid.empty()) return;

    for (const auto& icao : masters) {
        this->releaseMaster(icao, releaseCid);
    }
}

bool Server::isMaster(const std::string& icao) {
    std::lock_guard lock(m_stateLock);
    return m_masterAirports.find(icao) != m_masterAirports.end();
}

void Server::sendHeartbeats(const std::string& cid) {
    std::set<std::string> masters;
    {
        std::lock_guard lock(m_stateLock);
        masters = m_masterAirports;
    }

    if (masters.empty()) return;
    
    this->setCid(cid);

    for (const auto& icao : masters) {
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
            
            __receivedPatchData.clear();
            CURLcode result = curl_easy_perform(m_patchRequest.socket);

            long responseCode = 0;
            curl_easy_getinfo(m_patchRequest.socket, CURLINFO_RESPONSE_CODE, &responseCode);

            if (result != CURLE_OK || responseCode == 404 || responseCode == 409) {
                std::lock_guard lock(m_stateLock);
                m_masterAirports.erase(icao);
                m_errorCode = "Master heartbeat failed for " + icao + " (HTTP " + std::to_string(responseCode) + ")";
            }

            curl_easy_setopt(m_patchRequest.socket, CURLOPT_CUSTOMREQUEST, "PATCH"); // restore
            __receivedPatchData.clear();
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

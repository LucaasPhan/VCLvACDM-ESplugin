#include "DataManager.h"

#include "core/Server.h"
#include "log/Logger.h"
#include "main.h"
#include "utils/Date.h"

using namespace vacdm::com;
using namespace vacdm::core;
using namespace vacdm::logging;
using namespace std::chrono_literals;

static constexpr std::size_t ConsolidatedData = 0;
static constexpr std::size_t EuroscopeData = 1;
static constexpr std::size_t ServerData = 2;

DataManager::DataManager() : m_pause(false), m_stop(false) { this->m_worker = std::thread(&DataManager::run, this); }

DataManager::~DataManager() {
    this->m_stop = true;
    this->m_worker.join();
}

DataManager& DataManager::instance() {
    static DataManager __instance;
    return __instance;
}

bool DataManager::checkPilotExists(const std::string& callsign) {
    if (true == this->m_pause) return false;

    std::lock_guard guard(this->m_pilotLock);
    return this->m_pilots.cend() != this->m_pilots.find(callsign);
}

const types::Pilot DataManager::getPilot(const std::string& callsign) {
    std::lock_guard guard(this->m_pilotLock);
    return this->m_pilots.find(callsign)->second[ConsolidatedData];
}

void DataManager::pause() { this->m_pause = true; }

void DataManager::resume() { this->m_pause = false; }

std::string DataManager::setUpdateCycleSeconds(const int newUpdateCycleSeconds) {
    if (newUpdateCycleSeconds < minUpdateCycleSeconds || newUpdateCycleSeconds > maxUpdateCycleSeconds)
        return "Could not set update rate";

    this->updateCycleSeconds = newUpdateCycleSeconds;

    return "VCLvACDM updating every " +
           (newUpdateCycleSeconds == 1 ? "second" : std::to_string(newUpdateCycleSeconds) + " seconds");
}

void DataManager::run() {
    std::size_t counter = 1;
    while (true) {
        std::this_thread::sleep_for(1s);
        if (true == this->m_stop) return;
        if (true == this->m_pause) continue;

        // run every updateCycleSeconds seconds
        if (counter++ % updateCycleSeconds != 0) continue;

        // refresh airport metadata for all active airports
        {
            std::lock_guard guard(this->m_airportLock);
            for (const auto& icao : m_activeAirports) {
                com::Server::instance().refreshAirportMetadata(icao);
            }
        }

        // obtain a copy of the pilot data, work with the copy to minimize lock time
        std::map<std::string, std::array<vacdm::types::Pilot, 3U>> pilots;
        {
            std::lock_guard guard(this->m_pilotLock);
            pilots = this->m_pilots;
        }

        this->processAsynchronousMessages(pilots);

        this->processEuroScopeUpdates(pilots);

        this->consolidateWithBackend(pilots);

        std::list<std::tuple<types::Pilot, DataManager::MessageType, Json::Value>> transmissionBuffer;
        for (auto& pilot : pilots) {
            const auto& consolidatedPilot = pilot.second[ConsolidatedData];
            if (!Server::instance().isMaster(consolidatedPilot.origin)) {
                Logger::instance().log(Logger::LogSender::DataManager,
                                       "Skipping " + consolidatedPilot.callsign + ": not master for " +
                                           consolidatedPilot.origin,
                                       Logger::LogLevel::Debug);
                continue;
            }

            Json::Value message;
            const auto sendType = DataManager::deltaEuroscopeToBackend(pilot.second, message);
            if (MessageType::None != sendType) {
                transmissionBuffer.push_back({consolidatedPilot, sendType, message});
            } else {
                Logger::instance().log(Logger::LogSender::DataManager,
                                       "Skipping " + consolidatedPilot.callsign + ": no delta to send",
                                       Logger::LogLevel::Debug);
            }
        }

        for (const auto& transmission : std::as_const(transmissionBuffer)) {
            if (std::get<1>(transmission) == MessageType::Post)
                com::Server::instance().postPilot(std::get<0>(transmission));
            else if (std::get<1>(transmission) == MessageType::Patch)
                com::Server::instance().sendPatchMessage("/api/v1/pilots/" + std::get<0>(transmission).callsign,
                                                         std::get<2>(transmission));
        }

        {
            // replace the pilot data with the updated copy
            std::lock_guard guard(this->m_pilotLock);
            this->m_pilots = pilots;
        }
    }
}

void DataManager::processAsynchronousMessages(std::map<std::string, std::array<types::Pilot, 3U>>& pilots) {
    std::list<AsynchronousMessage> messages;
    {
        std::lock_guard guard(this->m_asyncMessagesLock);
        messages = std::move(this->m_asynchronousMessages);
    }

    for (auto& message : messages) {
        auto pilot = pilots.find(message.callsign);
        if (pilot == pilots.end()) continue;

        auto& [callsign, data] = *pilot;

        std::string messageType;

        switch (message.type) {
            case MessageType::UpdateEXOT:
                Server::instance().updateExot(message.callsign, message.value);
                messageType = "EXOT";
                break;
            case MessageType::UpdateTOBT:
                Server::instance().updateTobt(data[ConsolidatedData], message.value, false);
                messageType = "TOBT";
                break;
            case MessageType::UpdateTOBTConfirmed:
                Server::instance().updateTobt(data[ConsolidatedData], message.value, true);
                messageType = "TOBT Confirmed Status";
                break;
            case MessageType::UpdateASAT:
                Server::instance().updateAsat(message.callsign, message.value);
                messageType = "ASAT";
                break;
            case MessageType::UpdateASRT:
                Server::instance().updateAsrt(message.callsign, message.value);
                messageType = "ASRT";
                break;
            case MessageType::UpdateAOBT:
                Server::instance().updateAobt(message.callsign, message.value);
                messageType = "AOBT";
                break;
            case MessageType::UpdateAORT:
                Server::instance().updateAort(message.callsign, message.value);
                messageType = "AORT";
                break;
            case MessageType::ResetTOBT:
                Server::instance().resetTobt(message.callsign, types::defaultTime, data[ConsolidatedData].tobt_state);
                messageType = "TOBT reset";
                break;
            case MessageType::ResetASAT:
                Server::instance().updateAsat(message.callsign, message.value);
                messageType = "ASAT reset";
                break;
            case MessageType::ResetASRT:
                Server::instance().updateAsrt(message.callsign, message.value);
                messageType = "ASRT reset";
                break;
            case MessageType::ResetTOBTConfirmed:
                Server::instance().resetTobt(message.callsign, data[ConsolidatedData].tobt, "GUESS");
                messageType = "TOBT confirmed reset";
                break;
            case MessageType::ResetAORT:
                Server::instance().updateAort(message.callsign, message.value);
                messageType = "AORT reset";
                break;
            case MessageType::ResetAOBT:
                Server::instance().updateAobt(message.callsign, message.value);
                messageType = "AOBT reset";
                break;
            case MessageType::ResetPilot:
                Server::instance().deletePilot(message.callsign);
                pilots.erase(message.callsign);
                messageType = "Pilot reset";
                break;
            case MessageType::RemoveLocalPilot:
                pilots.erase(message.callsign);
                messageType = "Local pilot tracking removed";
                break;
            case MessageType::UpdateTSAC:
                Server::instance().updateTsac(message.callsign, message.value);
                messageType = "TSAC update";
                break;
            case MessageType::UpdateAOBTAuto:
                Server::instance().updateAobt(message.callsign, message.value);
                data[ConsolidatedData].aobt = message.value;
                data[EuroscopeData].aobt = message.value;
                messageType = "AOBT auto-recorded";
                break;
            case MessageType::UpdateATOT:
                {
                    Json::Value atotPatch;
                    atotPatch["vacdm"]["atot"] = utils::Date::timestampToIsoString(message.value);
                    Server::instance().sendPatchMessage("/api/v1/pilots/" + message.callsign, atotPatch);
                    data[ConsolidatedData].atot = message.value;
                    data[EuroscopeData].atot = message.value;
                    messageType = "ATOT auto-recorded";
                }
                break;

            default:
                break;
        }

        Logger::instance().log(Logger::LogSender::DataManager,
                               "Sending " + messageType + " update: " + message.callsign + " - " +
                                   utils::Date::timestampToIsoString(message.value),
                               Logger::LogLevel::Info);
    }
}

void DataManager::handleTagFunction(MessageType type, const std::string callsign,
                                    const std::chrono::utc_clock::time_point value) {
    // do not handle the tag function if the aircraft does not exist
    if (false == this->checkPilotExists(callsign)) return;

    const auto currentPilot = this->getPilot(callsign);
    if (!Server::instance().isMaster(currentPilot.origin)) return;

    // queue the update message which will be sent to the backend
    {
        std::lock_guard guard(this->m_asyncMessagesLock);
        this->m_asynchronousMessages.push_back({type, callsign, value});
    }

    // set the data locally, gives feedback to user that the action was handled, might get overwritten again in the
    // update cycle if the backend does not accept the message
    std::lock_guard guard(this->m_pilotLock);
    auto it = this->m_pilots.find(callsign);
    auto& pilot = it->second[ConsolidatedData];

    pilot.lastUpdate = std::chrono::utc_clock::now();

    switch (type) {
        case MessageType::UpdateEXOT:
            pilot.exot = value;
            pilot.tsat = types::defaultTime;
            pilot.ttot = types::defaultTime;
            pilot.asat = types::defaultTime;
            pilot.aobt = types::defaultTime;
            pilot.atot = types::defaultTime;
            break;
        case MessageType::UpdateTOBT: {
            bool resetTsat = value >= pilot.tsat;

            pilot.tobt = value;
            if (true == resetTsat) pilot.tsat = types::defaultTime;
            pilot.ttot = types::defaultTime;
            pilot.exot = types::defaultTime;
            pilot.asat = types::defaultTime;
            pilot.aobt = types::defaultTime;
            pilot.atot = types::defaultTime;

            break;
        }
        case MessageType::UpdateTOBTConfirmed: {
            bool resetTsat = value == types::defaultTime || value >= pilot.tsat;

            pilot.tobt = value;
            if (true == resetTsat) pilot.tsat = types::defaultTime;
            pilot.ttot = types::defaultTime;
            pilot.exot = types::defaultTime;
            pilot.asat = types::defaultTime;
            pilot.aobt = types::defaultTime;
            pilot.atot = types::defaultTime;

            break;
        }
        case MessageType::UpdateASAT:
            pilot.asat = value;
            break;
        case MessageType::UpdateASRT:
            pilot.asrt = value;
            break;
        case MessageType::UpdateAOBT:
            pilot.aobt = value;
            break;
        case MessageType::UpdateAORT:
            pilot.aort = value;
            break;
        case MessageType::ResetTOBT:
            pilot.tobt = types::defaultTime;
            pilot.tsat = types::defaultTime;
            pilot.ttot = types::defaultTime;
            pilot.exot = types::defaultTime;
            pilot.asat = types::defaultTime;
            pilot.asrt = types::defaultTime;
            pilot.aobt = types::defaultTime;
            pilot.aort = types::defaultTime;
            pilot.atot = types::defaultTime;
            break;
        case MessageType::ResetASAT:
            pilot.asat = types::defaultTime;
            break;
        case MessageType::ResetASRT:
            pilot.asrt = types::defaultTime;
            break;
        case MessageType::ResetTOBTConfirmed:
            pilot.tobt_state = "GUESS";
            break;
        case MessageType::ResetAORT:
            pilot.aort = types::defaultTime;
            break;
        case MessageType::ResetAOBT:
            pilot.aobt = types::defaultTime;
            break;
        case MessageType::ResetPilot:
            pilot.eobt = types::defaultTime;
            pilot.tobt = types::defaultTime;
            pilot.ctot = types::defaultTime;
            pilot.ttot = types::defaultTime;
            pilot.tsat = types::defaultTime;
            pilot.exot = types::defaultTime;
            pilot.asat = types::defaultTime;
            pilot.aobt = types::defaultTime;
            pilot.atot = types::defaultTime;
            pilot.asrt = types::defaultTime;
            pilot.aort = types::defaultTime;
            break;
        case MessageType::UpdateTSAC:
            if (value == types::defaultTime) {
                pilot.tsac = "";
            } else {
                char buf[10];
                std::snprintf(buf, sizeof(buf), "%02d%02d", 
                              (int)std::chrono::duration_cast<std::chrono::hours>(value.time_since_epoch() % std::chrono::hours(24)).count(),
                              (int)std::chrono::duration_cast<std::chrono::minutes>(value.time_since_epoch() % std::chrono::hours(1)).count());
                pilot.tsac = buf;
            }
            break;
        default:
            break;
    }
}

DataManager::MessageType DataManager::deltaEuroscopeToBackend(const std::array<types::Pilot, 3>& data,
                                                              Json::Value& message) {
    message.clear();

    if (data[ServerData].callsign == "" && data[EuroscopeData].callsign != "") {
        return DataManager::MessageType::Post;
    } else {
        message["callsign"] = data[EuroscopeData].callsign;

        int deltaCount = 0;

        if (data[EuroscopeData].inactive != data[ServerData].inactive) {
            message["inactive"] = data[EuroscopeData].inactive;
            deltaCount += 1;
        }

        if (data[EuroscopeData].onGround != data[ServerData].onGround) {
            message["onGround"] = data[EuroscopeData].onGround;
            deltaCount += 1;
        }

        if (data[EuroscopeData].aircraft != data[ServerData].aircraft) {
            message["aircraft"] = data[EuroscopeData].aircraft;
            deltaCount += 1;
        }

        auto lastDelta = deltaCount;
        message["position"] = Json::Value();
        if (data[EuroscopeData].onGround) {
            if (data[EuroscopeData].latitude != data[ServerData].latitude) {
                message["position"]["lat"] = data[EuroscopeData].latitude;
                deltaCount += 1;
            }
            if (data[EuroscopeData].longitude != data[ServerData].longitude) {
                message["position"]["lon"] = data[EuroscopeData].longitude;
                deltaCount += 1;
            }
        }
        if (deltaCount == lastDelta) message.removeMember("position");

        // patch flightplan data
        lastDelta = deltaCount;
        message["flightplan"] = Json::Value();
        if (data[EuroscopeData].origin != data[ServerData].origin) {
            deltaCount += 1;
            message["flightplan"]["departure"] = data[EuroscopeData].origin;
        }
        if (data[EuroscopeData].destination != data[ServerData].destination) {
            deltaCount += 1;
            message["flightplan"]["arrival"] = data[EuroscopeData].destination;
        }
        if (data[EuroscopeData].flightType != data[ServerData].flightType) {
            deltaCount += 1;
            message["flightplan"]["flightType"] = data[EuroscopeData].flightType;
        }
        if (deltaCount == lastDelta) message.removeMember("flightplan");

        // patch clearance data
        lastDelta = deltaCount;
        message["clearance"] = Json::Value();
        if (data[EuroscopeData].runway != data[ServerData].runway) {
            deltaCount += 1;
            message["clearance"]["dep_rwy"] = data[EuroscopeData].runway;
        }
        if (data[EuroscopeData].sid != data[ServerData].sid) {
            deltaCount += 1;
            message["clearance"]["sid"] = data[EuroscopeData].sid;
        }
        if (deltaCount == lastDelta) message.removeMember("clearance");

        return deltaCount != 0 ? DataManager::MessageType::Patch : DataManager::MessageType::None;
    }
}

void DataManager::setActiveAirports(const std::list<std::string> activeAirports) {
    std::list<std::string> supportedAirports;
    for (const auto& icao : activeAirports) {
        if (com::Server::instance().isSupportedAirport(icao)) {
            supportedAirports.push_back(icao);
        } else {
            Logger::instance().log(Logger::LogSender::DataManager, "Ignoring unsupported active airport: " + icao,
                                   Logger::LogLevel::Info);
        }
    }

    std::lock_guard guard(this->m_airportLock);
    this->m_activeAirports = supportedAirports;

    // Clear purged cache on airport change
    {
        std::lock_guard guard2(this->m_euroscopeUpdatesLock);
        this->m_backendPurgedCallsigns.clear();
    }
}

std::list<std::string> DataManager::getActiveAirports() {
    std::lock_guard guard(this->m_airportLock);
    return this->m_activeAirports;
}

void DataManager::queueFlightplanUpdate(EuroScopePlugIn::CFlightPlan flightplan) {
    // skip the update if:
    // - the flightplan or its data is invalid
    if (false == flightplan.IsValid() || nullptr == flightplan.GetFlightPlanData().GetPlanType() ||
        nullptr == flightplan.GetFlightPlanData().GetOrigin())
        return;

    // skip if not connected to the network / no radar target
    if (!Plugin->RadarTargetSelect(flightplan.GetCallsign()).IsValid()) {
        return;
    }

    auto pilot = this->CFlightPlanToPilot(flightplan);

    std::lock_guard guard(this->m_euroscopeUpdatesLock);
    if (this->m_backendPurgedCallsigns.find(pilot.callsign) != this->m_backendPurgedCallsigns.end()) {
        Logger::instance().log(Logger::LogSender::DataManager,
                               "Ignoring " + pilot.callsign + ": pilot was purged from backend",
                               Logger::LogLevel::Debug);
        return;
    }
    this->m_euroscopeFlightplanUpdates.push_back({std::chrono::utc_clock::now(), pilot});
}

void DataManager::prunePurgedCache(const std::set<std::string>& activeCallsigns) {
    std::lock_guard guard(this->m_euroscopeUpdatesLock);
    for (auto it = m_backendPurgedCallsigns.begin(); it != m_backendPurgedCallsigns.end(); ) {
        if (activeCallsigns.find(*it) == activeCallsigns.end()) {
            Logger::instance().log(Logger::LogSender::DataManager, "Pruning " + *it + " from purged cache", Logger::LogLevel::Debug);
            it = m_backendPurgedCallsigns.erase(it);
        } else {
            ++it;
        }
    }
}

void DataManager::handleDisconnectedFlights(const std::set<std::string>& activeCallsigns) {
    std::lock_guard guard(this->m_pilotLock);
    for (const auto& pair : this->m_pilots) {
        if (activeCallsigns.find(pair.first) == activeCallsigns.end()) {
            // Check if we haven't already queued a removal for this callsign
            bool alreadyQueued = false;
            {
                std::lock_guard asyncGuard(this->m_asyncMessagesLock);
                for (const auto& msg : this->m_asynchronousMessages) {
                    if ((msg.type == MessageType::ResetPilot || msg.type == MessageType::RemoveLocalPilot) && msg.callsign == pair.first) {
                        alreadyQueued = true;
                        break;
                    }
                }
                if (!alreadyQueued) {
                    Logger::instance().log(Logger::LogSender::DataManager,
                                           "Pilot disconnected, queueing local removal: " + pair.first,
                                           Logger::LogLevel::Info);
                    this->m_asynchronousMessages.push_back({MessageType::RemoveLocalPilot, pair.first, std::chrono::utc_clock::now()});
                }
            }
        }
    }
}

void DataManager::consolidateWithBackend(std::map<std::string, std::array<types::Pilot, 3U>>& pilots) {
    // retrieving backend data
    auto backendPilots = Server::instance().getPilots(this->m_activeAirports);
    const bool backendFetchOk = Server::instance().lastPilotFetchOk();

    for (auto pilot = pilots.begin(); pilots.end() != pilot;) {
        // update backend data & consolidate
        bool removeFlight = pilot->second[ServerData].inactive == true;
        bool foundInBackend = false;
        for (auto updateIt = backendPilots.begin(); updateIt != backendPilots.end(); ++updateIt) {
            if (updateIt->callsign == pilot->second[EuroscopeData].callsign) {
                Logger::instance().log(
                    Logger::LogSender::DataManager,
                    "Updating " + pilot->second[EuroscopeData].callsign + " with" + updateIt->callsign,
                    Logger::LogLevel::Info);
                pilot->second[ServerData] = *updateIt;
                DataManager::consolidateData(pilot->second);
                removeFlight = false;
                foundInBackend = true;
                
                // Clear from purged cache if found again in backend
                {
                    std::lock_guard guard(this->m_euroscopeUpdatesLock);
                    this->m_backendPurgedCallsigns.erase(updateIt->callsign);
                }

                updateIt = backendPilots.erase(updateIt);
                break;
            }
        }

        if (backendFetchOk && !foundInBackend && !pilot->second[ServerData].callsign.empty()) {
            Logger::instance().log(Logger::LogSender::DataManager,
                                   "Removing " + pilot->second[EuroscopeData].callsign +
                                       ": pilot disappeared from backend",
                                   Logger::LogLevel::Info);
            {
                std::lock_guard guard(this->m_euroscopeUpdatesLock);
                this->m_backendPurgedCallsigns.insert(pilot->second[EuroscopeData].callsign);
            }
            removeFlight = true;
        }

        // remove pilot if he has been flagged as inactive or purged from the backend
        if (true == removeFlight) {
            pilot = pilots.erase(pilot);
        } else {
            ++pilot;
        }
    }

    // handle remaining backendPilots (newly added or re-activated)
    for (const auto& backendPilot : backendPilots) {
        std::lock_guard guard(this->m_euroscopeUpdatesLock);
        this->m_backendPurgedCallsigns.erase(backendPilot.callsign);
    }
}

void DataManager::consolidateData(std::array<types::Pilot, 3>& pilot) {
    if (pilot[EuroscopeData].callsign == pilot[ServerData].callsign) {
        // backend data
        pilot[ConsolidatedData].inactive = pilot[ServerData].inactive;
        pilot[ConsolidatedData].lastUpdate = pilot[ServerData].lastUpdate;

        pilot[ConsolidatedData].eobt = pilot[ServerData].eobt;
        pilot[ConsolidatedData].tobt = pilot[ServerData].tobt;
        pilot[ConsolidatedData].tobt_state = pilot[ServerData].tobt_state;
        pilot[ConsolidatedData].ctot = pilot[ServerData].ctot;
        pilot[ConsolidatedData].ttot = pilot[ServerData].ttot;
        pilot[ConsolidatedData].tsat = pilot[ServerData].tsat;
        pilot[ConsolidatedData].exot = pilot[ServerData].exot;
        pilot[ConsolidatedData].asat = pilot[ServerData].asat;
        pilot[ConsolidatedData].aobt = pilot[ServerData].aobt;
        pilot[ConsolidatedData].atot = pilot[ServerData].atot;
        pilot[ConsolidatedData].asrt = pilot[ServerData].asrt;
        pilot[ConsolidatedData].aort = pilot[ServerData].aort;
        pilot[ConsolidatedData].tsatReset = pilot[ServerData].tsatReset;

        pilot[ConsolidatedData].hasBooking = pilot[ServerData].hasBooking;
        pilot[ConsolidatedData].taxizoneIsTaxiout = pilot[ServerData].taxizoneIsTaxiout;

        // EuroScope data
        pilot[ConsolidatedData].latitude = pilot[EuroscopeData].latitude;
        pilot[ConsolidatedData].longitude = pilot[EuroscopeData].longitude;
        pilot[ConsolidatedData].onGround = pilot[EuroscopeData].onGround;

        pilot[ConsolidatedData].origin = pilot[EuroscopeData].origin;
        pilot[ConsolidatedData].destination = pilot[EuroscopeData].destination;
        pilot[ConsolidatedData].runway = pilot[EuroscopeData].runway;
        pilot[ConsolidatedData].sid = pilot[EuroscopeData].sid;
        pilot[ConsolidatedData].aircraft = pilot[EuroscopeData].aircraft;
        pilot[ConsolidatedData].flightType = pilot[EuroscopeData].flightType;
        pilot[ConsolidatedData].airline = pilot[EuroscopeData].airline;

        pilot[ConsolidatedData].exemptFromCdm = pilot[ServerData].exemptFromCdm;
        pilot[ConsolidatedData].groundHandler = pilot[ServerData].groundHandler;

        logging::Logger::instance().log(Logger::LogSender::DataManager, "Consolidated " + pilot[ServerData].callsign,
                                        logging::Logger::LogLevel::Info);
    } else {
        logging::Logger::instance().log(Logger::LogSender::DataManager,
                                        "Callsign mismatch during consolidation: " + pilot[EuroscopeData].callsign +
                                            ", " + pilot[ServerData].callsign,
                                        logging::Logger::LogLevel::Critical);
    }
}

void DataManager::processEuroScopeUpdates(std::map<std::string, std::array<types::Pilot, 3U>>& pilots) {
    // obtain a copy of the flightplan updates, clear the update list, consolidate flightplan updates
    std::list<EuroscopeFlightplanUpdate> flightplanUpdates;
    {
        std::lock_guard guard(this->m_euroscopeUpdatesLock);
        flightplanUpdates.swap(this->m_euroscopeFlightplanUpdates);
    }

    this->consolidateFlightplanUpdates(flightplanUpdates);

    for (auto& update : flightplanUpdates) {
        const auto& pilot = update.data;

        auto it = pilots.find(pilot.callsign);

        if (it != pilots.end()) {
            // Pilot found, update the corresponding data
            Logger::instance().log(Logger::LogSender::DataManager, "Updated data of " + pilot.callsign,
                                   Logger::LogLevel::Info);

            const auto& prevES = it->second[EuroscopeData];
            const std::string prevGS = prevES.groundState;
            const std::string newGS  = pilot.groundState;
            const auto now = std::chrono::utc_clock::now();

            auto updatedPilot = pilot;

            // Carry over already-recorded AOBT/ATOT so they are never reset
            if (prevES.aobt != types::defaultTime) updatedPilot.aobt = prevES.aobt;
            if (prevES.atot != types::defaultTime) updatedPilot.atot = prevES.atot;

            // if airborne, stop tracking position and keep last known ground position
            if (!updatedPilot.onGround) {
                updatedPilot.latitude = prevES.latitude;
                updatedPilot.longitude = prevES.longitude;
            }

            // --- AOBT auto-recording (STUP / PUSH transition) ---
            if (updatedPilot.aobt == types::defaultTime) {
                bool wasMoving = (prevGS == "STUP" || prevGS == "PUSH");
                bool isMoving  = (newGS  == "STUP" || newGS  == "PUSH");
                if (!wasMoving && isMoving) {
                    Logger::instance().log(Logger::LogSender::DataManager,
                                           "[" + pilot.callsign + "] Auto-recording AOBT on " + newGS,
                                           Logger::LogLevel::Info);
                    updatedPilot.aobt = now;
                    std::lock_guard asyncGuard(this->m_asyncMessagesLock);
                    this->m_asynchronousMessages.push_back({MessageType::UpdateAOBTAuto, pilot.callsign, now});
                }
            }

            // --- ATOT auto-recording (TAKE OFF / DEPA transition) ---
            if (updatedPilot.atot == types::defaultTime) {
                bool wasTakeOff = (prevGS == "TAKE OFF" || prevGS == "DEPA");
                bool isTakeOff  = (newGS  == "TAKE OFF" || newGS  == "DEPA");
                if (!wasTakeOff && isTakeOff) {
                    Logger::instance().log(Logger::LogSender::DataManager,
                                           "[" + pilot.callsign + "] Auto-recording ATOT on " + newGS,
                                           Logger::LogLevel::Info);
                    updatedPilot.atot = now;
                    std::lock_guard asyncGuard(this->m_asyncMessagesLock);
                    this->m_asynchronousMessages.push_back({MessageType::UpdateATOT, pilot.callsign, now});
                }
            }

            // --- READY status sync: queue ground-state change if server says READY ---
            {
                const auto& serverData = it->second[ServerData];
                if (serverData.tobt_state == "READY" && newGS != "READY") {
                    std::lock_guard actionGuard(this->m_euroscopeActionsLock);
                    // Only queue once
                    bool alreadyQueued = false;
                    for (const auto& a : this->m_euroscopeActions) {
                        if (a.callsign == pilot.callsign) { alreadyQueued = true; break; }
                    }
                    if (!alreadyQueued) {
                        Logger::instance().log(Logger::LogSender::DataManager,
                                               "[" + pilot.callsign + "] Queueing READY ground-state sync",
                                               Logger::LogLevel::Info);
                        this->m_euroscopeActions.push_back({pilot.callsign, "READY"});
                    }
                }
            }

            it->second[EuroscopeData] = updatedPilot;
        } else {
            // Pilot not found, add a new entry
            Logger::instance().log(Logger::LogSender::DataManager,
                                   "Added new pilot entry for callsign: " + pilot.callsign, Logger::LogLevel::Info);
            pilots.insert({pilot.callsign, std::array<types::Pilot, 3U>{pilot, pilot, types::Pilot()}});
        }
    }
}

void DataManager::consolidateFlightplanUpdates(std::list<EuroscopeFlightplanUpdate>& inputList) {
    std::list<DataManager::EuroscopeFlightplanUpdate> resultList;

    for (const auto& currentUpdate : inputList) {
        auto pilot = currentUpdate.data;

        // only handle updates for active airports
        {
            std::lock_guard guard(this->m_airportLock);
            bool flightDepartsFromActiveAirport = std::find(m_activeAirports.begin(), m_activeAirports.end(),
                                                            std::string(pilot.origin)) != m_activeAirports.end();
            if (false == flightDepartsFromActiveAirport) {
                Logger::instance().log(Logger::LogSender::DataManager,
                                       "Ignoring " + pilot.callsign + ": origin " + pilot.origin +
                                           " is not an active supported airport",
                                       Logger::LogLevel::Debug);
                continue;
            }
        }

        // Check if the flight plan already exists in the result list
        auto it = std::find_if(resultList.begin(), resultList.end(),
                               [&currentUpdate](const EuroscopeFlightplanUpdate& existingUpdate) {
                                   return existingUpdate.data.callsign == currentUpdate.data.callsign;
                               });

        if (it != resultList.end()) {
            // Flight plan with the same callsign exists
            // Check if the timeIssued is newer
            if (currentUpdate.timeIssued > it->timeIssued) {
                // Update with the newer data
                *it = currentUpdate;
                Logger::instance().log(Logger::LogSender::DataManager,
                                       "Updated: " + std::string(currentUpdate.data.callsign), Logger::LogLevel::Info);
            } else {
                // Existing data is already newer, no update needed
                Logger::instance().log(Logger::LogSender::DataManager,
                                       "Skipped old update for: " + std::string(currentUpdate.data.callsign),
                                       Logger::LogLevel::Info);
            }
        } else {
            // Flight plan with the callsign doesn't exist, add it to the result list
            resultList.push_back(currentUpdate);
            Logger::instance().log(Logger::LogSender::DataManager,
                                   "Update added: " + std::string(currentUpdate.data.callsign), Logger::LogLevel::Info);
        }
    }

    inputList = resultList;
}

types::Pilot DataManager::CFlightPlanToPilot(const EuroScopePlugIn::CFlightPlan flightplan) {
    types::Pilot pilot;

    pilot.callsign = flightplan.GetCallsign();
    pilot.lastUpdate = std::chrono::utc_clock::now();

    // position data
    auto target = Plugin->RadarTargetSelect(pilot.callsign.c_str());
    if (target.IsValid()) {
        // SDK v16 does not provide GetOnGround(), use GS < 50 and altitude < 500ft as heuristic
        pilot.onGround = target.GetGS() < 50 && target.GetPosition().GetPressureAltitude() < 500;
        // stop tracking position if airborne
        if (pilot.onGround) {
            pilot.latitude = target.GetPosition().GetPosition().m_Latitude;
            pilot.longitude = target.GetPosition().GetPosition().m_Longitude;
        }
    } else {
        // if we have no radar target we will use the fptrackposition,
        // not sufficient precision to determine the taxizone
        pilot.latitude = flightplan.GetFPTrackPosition().GetPosition().m_Latitude;
        pilot.longitude = flightplan.GetFPTrackPosition().GetPosition().m_Longitude;
        pilot.onGround = true;
    }

    // flightplan & clearance data
    const char* origin = flightplan.GetFlightPlanData().GetOrigin();
    pilot.origin = (origin != nullptr) ? origin : "";
    
    const char* destination = flightplan.GetFlightPlanData().GetDestination();
    pilot.destination = (destination != nullptr) ? destination : "";
    
    const char* runway = flightplan.GetFlightPlanData().GetDepartureRwy();
    pilot.runway = (runway != nullptr) ? runway : "";

    const char* sidName = flightplan.GetFlightPlanData().GetSidName();
    pilot.sid = (sidName != nullptr) ? sidName : "";

    // fallback for TopSky/GRP: check scratchpad if SID is empty
    if (pilot.sid.empty()) {
        const char* scratchpad = flightplan.GetControllerAssignedData().GetScratchPadString();
        if (scratchpad != nullptr && std::strlen(scratchpad) > 0) {
            // common format: SID is the first word or the whole string
            std::string sp(scratchpad);
            size_t space = sp.find(' ');
            pilot.sid = (space != std::string::npos) ? sp.substr(0, space) : sp;
        }
    }

    if (!pilot.sid.empty()) {
        logging::Logger::instance().log(logging::Logger::LogSender::DataManager,
                                        "Extracted SID for " + pilot.callsign + ": " + pilot.sid,
                                        logging::Logger::LogLevel::Info);
    }

    const char* aircraft = flightplan.GetFlightPlanData().GetAircraftFPType();
    pilot.aircraft = (aircraft != nullptr) ? aircraft : "";

    const bool isDomestic = pilot.origin.rfind("VV", 0) == 0 && pilot.destination.rfind("VV", 0) == 0;
    pilot.flightType = isDomestic ? "DOMESTIC" : "INTERNATIONAL";

    if (pilot.callsign.length() >= 3) {
        pilot.airline = pilot.callsign.substr(0, 3);
    }

    // ground state (from GRP / TopSky)
    const char* gs = flightplan.GetGroundState();
    pilot.groundState = (gs != nullptr) ? gs : "";

    // acdm data
    pilot.eobt = utils::Date::convertEuroscopeDepartureTime(flightplan);
    pilot.tobt = pilot.eobt;

    return pilot;
}

std::list<DataManager::EuroScopeAction> DataManager::popEuroScopeActions() {
    std::lock_guard guard(this->m_euroscopeActionsLock);
    std::list<EuroScopeAction> actions;
    actions.swap(this->m_euroscopeActions);
    return actions;
}

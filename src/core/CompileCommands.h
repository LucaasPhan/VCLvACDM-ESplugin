#include <algorithm>
#include <string>

#pragma warning(push, 0)
#include "EuroScopePlugIn.h"
#pragma warning(pop)

#include "core/DataManager.h"
#include "core/Server.h"
#include "log/Logger.h"
#include "utils/Number.h"
#include "utils/String.h"
#include "utils/Date.h"
#include "vACDM.h"

using namespace vacdm;
using namespace vacdm::logging;
using namespace vacdm::core;
using namespace vacdm::utils;

namespace vacdm {
bool vACDM::OnCompileCommand(const char *sCommandLine) {
    std::string command(sCommandLine);

#pragma warning(push)
#pragma warning(disable : 4244)
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
#pragma warning(pop)

    // only handle commands containing ".acdm"
    if (0 != command.find(".ACDM")) return false;

    // master command
    if (std::string::npos != command.find("MASTER")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .acdm MASTER <ICAO>");
            return true;
        }
        std::string icao = elements[2];

        if (icao != "VVTS" && icao != "VVNB") {
            DisplayMessage("VCLvACDM is currently only supported at VVTS and VVNB");
            return true;
        }

        bool userIsConnected = this->GetConnectionType() != EuroScopePlugIn::CONNECTION_TYPE_NO;
        bool userIsInSweatbox = this->GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_SWEATBOX;
        bool userIsObserver = std::string_view(this->ControllerMyself().GetCallsign()).ends_with("_OBS") == true ||
                              this->ControllerMyself().GetFacility() == 0;
        bool serverAllowsObsAsMaster = com::Server::instance().getServerConfig().allowMasterAsObserver;
        bool serverAllowsSweatboxAsMaster = com::Server::instance().getServerConfig().allowMasterInSweatbox;

        std::string userIsNotEligibleMessage;

        if (!userIsConnected && !this->m_debugMode) {
            userIsNotEligibleMessage = "You are not logged in to the VATSIM network";
        } else if (userIsObserver && !serverAllowsObsAsMaster) {
            userIsNotEligibleMessage = "You are logged in as Observer and Server does not allow Observers to be Master";
        } else if (userIsInSweatbox && !serverAllowsSweatboxAsMaster) {
            userIsNotEligibleMessage =
                "You are logged in on a Sweatbox Server and Server does not allow Sweatbox connections";
        } else if (!com::Server::instance().isSupportedAirport(icao)) {
            userIsNotEligibleMessage = icao + " is not supported by this VCLvACDM deployment";
        } else {
            com::Server::instance().refreshAirportMetadata(icao);
            const auto meta = com::Server::instance().getAirportMetadata(icao);
            const std::string callsign = this->ControllerMyself().GetCallsign();

            if (!meta.master.empty() && meta.master != callsign && !com::Server::instance().isMaster(icao)) {
                DisplayMessage("Cannot upgrade to Master");
                DisplayMessage(icao + " is already managed by " + meta.master);
                return true;
            }

            DisplayMessage("Claiming ACDM MASTER for " + icao);
            Logger::instance().log(Logger::LogSender::vACDM, "Claiming MASTER for " + icao, Logger::LogLevel::Info);
            com::Server::instance().claimMaster(icao, callsign, callsign);
            
            std::string err = com::Server::instance().errorMessage();
            if (!err.empty()) {
                DisplayMessage(err);
            } else if (com::Server::instance().isMaster(icao)) {
                DisplayMessage("ACDM MASTER claim successful for " + icao);
                this->OnAirportRunwayActivityChanged();
                this->runEuroscopeUpdate();
            }
            return true;
        }

        DisplayMessage("Cannot upgrade to Master");
        DisplayMessage(userIsNotEligibleMessage);
        return true;
    } else if (std::string::npos != command.find("SLAVE")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        
        if (elements.size() < 3 || elements[2] == "ALL") {
            DisplayMessage("Releasing all ACDM MASTER claims");
            Logger::instance().log(Logger::LogSender::vACDM, "Releasing all MASTER claims", Logger::LogLevel::Info);
            com::Server::instance().releaseAllMasters(this->ControllerMyself().GetCallsign());
            this->OnAirportRunwayActivityChanged();
            DisplayMessage("All ACDM MASTER claims released");
            return true;
        }

        std::string icao = elements[2];

        if (icao != "VVTS" && icao != "VVNB") {
            DisplayMessage("ACDM is currently only supported at VVTS and VVNB");
            return true;
        }

        DisplayMessage("Releasing ACDM MASTER for " + icao);
        Logger::instance().log(Logger::LogSender::vACDM, "Releasing MASTER for " + icao, Logger::LogLevel::Info);
        com::Server::instance().releaseMaster(icao, this->ControllerMyself().GetCallsign());
        
        std::string err = com::Server::instance().errorMessage();
        if (!err.empty()) {
            DisplayMessage(err);
        } else {
            DisplayMessage("ACDM MASTER released for " + icao);
            this->OnAirportRunwayActivityChanged();
        }
        return true;
    } else if (std::string::npos != command.find("RELOAD")) {
        this->reloadConfiguration();
        return true;
    } else if (std::string::npos != command.find("DEBUG")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .acdm DEBUG ON/OFF");
            return true;
        }
        if (elements[2] == "ON") {
            this->m_debugMode = true;
            DisplayMessage("Debug mode ON: Polling active while disconnected.");
        } else {
            this->m_debugMode = false;
            DisplayMessage("Debug mode OFF: Polling disabled while disconnected.");
        }
        return true;
    } else if (std::string::npos != command.find("LOG")) {
        if (std::string::npos != command.find("LOGLEVEL")) {
            DisplayMessage(Logger::instance().handleLogLevelCommand(command));
        } else {
            DisplayMessage(Logger::instance().handleLogCommand(command));
        }
        return true;
    } else if (std::string::npos != command.find("UNEXEMPT")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .acdm UNEXEMPT <CALLSIGN>");
            return true;
        }
        Json::Value root;
        root["callsign"] = elements[2];
        root["exemptFromCdm"] = false;
        com::Server::instance().sendPatchMessage("/api/v1/pilots/" + elements[2], root);
        DisplayMessage(elements[2] + " removed from CDM-exempt");
        return true;
    } else if (std::string::npos != command.find("EXEMPT")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .acdm EXEMPT <CALLSIGN>");
            return true;
        }
        Json::Value root;
        root["callsign"] = elements[2];
        root["exemptFromCdm"] = true;
        com::Server::instance().sendPatchMessage("/api/v1/pilots/" + elements[2], root);
        DisplayMessage(elements[2] + " marked as CDM-exempt (VIP/medical/SAR)");
        return true;
    } else if (std::string::npos != command.find("LVO")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 4) {
            DisplayMessage("Usage: .acdm LVO <ICAO> ON/OFF");
            return true;
        }
        std::string icao = elements[2];
        std::string state = elements[3];

        if (icao != "VVTS" && icao != "VVNB") {
            DisplayMessage("LVO command is only supported for VVTS and VVNB");
            return true;
        }

        if (this->GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_NO && !this->m_debugMode) {
            DisplayMessage("You must be connected to the network to use this command.");
            return true;
        }

        if (!com::Server::instance().isMaster(icao)) {
            DisplayMessage("You must be MASTER of " + icao + " to toggle LVO.");
            return true;
        }

        bool active = (state == "ON");
        com::Server::instance().toggleLvo(icao, active);
        DisplayMessage("LVO " + std::string(active ? "activated" : "deactivated") + " for " + icao);
        return true;
    } else if (std::string::npos != command.find("STARTUPDELAY") || std::string::npos != command.find("DEPARTUREDELAY")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 4) {
            DisplayMessage("Usage: .acdm STARTUPDELAY <ICAO>/<RWY> <TIME>");
            return true;
        }
        
        std::string type = (command.find("STARTUPDELAY") != std::string::npos) ? "startup" : "departure";
        std::string icao_rwy = elements[2];
        std::string time = elements[3];
        
        auto slashPos = icao_rwy.find("/");
        if (slashPos == std::string::npos) {
            DisplayMessage("Invalid format. Use <ICAO>/<RWY> e.g. VVTS/25L");
            return true;
        }
        std::string icao = icao_rwy.substr(0, slashPos);
        std::string rwy = icao_rwy.substr(slashPos + 1);

        if (this->GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_NO && !this->m_debugMode) {
            DisplayMessage("You must be connected to the network to use this command.");
            return true;
        }

        if (!com::Server::instance().isMaster(icao)) {
            DisplayMessage("You must be MASTER of " + icao + " to set delays.");
            return true;
        }

        // Handle time (absolute or relative)
        if (time == "9999") {
            std::string url = "/api/v1/airports/" + icao + "/delays?runway=" + rwy + "&type=" + type;
            com::Server::instance().sendDeleteMessage(url);
            DisplayMessage("Delay removed for " + icao + " " + rwy);
        } else {
            std::string absoluteTime = time;
            if (time.length() <= 2) { // relative
                int mins = std::stoi(time);
                auto future = std::chrono::utc_clock::now() + std::chrono::minutes(mins);
                char buf[10];
                std::snprintf(buf, sizeof(buf), "%02d%02d", 
                              (int)std::chrono::duration_cast<std::chrono::hours>(future.time_since_epoch() % std::chrono::hours(24)).count(),
                              (int)std::chrono::duration_cast<std::chrono::minutes>(future.time_since_epoch() % std::chrono::hours(1)).count());
                absoluteTime = buf;
            }
            
            auto timePoint = utils::Date::convertStringToTimePoint(absoluteTime);
            std::string isoTime = utils::Date::timestampToIsoString(timePoint);

            com::Server::instance().postDelay(icao, rwy, type, isoTime);
            DisplayMessage("Delay set for " + icao + " " + rwy + " from " + absoluteTime + "z");
        }
        return true;
    } else if (std::string::npos != command.find("REACTIVATE")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .acdm REACTIVATE <CALLSIGN>");
            return true;
        }
        std::string callsign = elements[2];
        EuroScopePlugIn::CFlightPlan fp = this->FlightPlanSelect(callsign.c_str());
        if (!fp.IsValid()) {
            DisplayMessage("Callsign " + callsign + " not found in EuroScope.");
            return true;
        }

        double lat = fp.GetFPTrackPosition().GetPosition().m_Latitude;
        double lon = fp.GetFPTrackPosition().GetPosition().m_Longitude;
        
        com::Server::instance().probeParkingStand(callsign, lat, lon);
        DisplayMessage("Re-activation probe sent for " + callsign);
        return true;
    } else if (std::string::npos != command.find("PANEL")) {
        vacdm::core::StatusPanel::pluginConfig.showPanel = true;
        DisplayMessage("ACDM Panel is now visible");
        return true;
    }
    return false;
}
}  // namespace vacdm

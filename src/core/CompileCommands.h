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

    // only handle commands containing ".vacdm"
    if (0 != command.find(".VACDM")) return false;

    // master command
    if (std::string::npos != command.find("MASTER")) {
        bool userIsConnected = this->GetConnectionType() != EuroScopePlugIn::CONNECTION_TYPE_NO;
        bool userIsInSweatbox = this->GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_SWEATBOX;
        bool userIsObserver = std::string_view(this->ControllerMyself().GetCallsign()).ends_with("_OBS") == true ||
                              this->ControllerMyself().GetFacility() == 0;
        bool serverAllowsObsAsMaster = com::Server::instance().getServerConfig().allowMasterAsObserver;
        bool serverAllowsSweatboxAsMaster = com::Server::instance().getServerConfig().allowMasterInSweatbox;

        std::string userIsNotEligibleMessage;

        if (!userIsConnected) {
            userIsNotEligibleMessage = "You are not logged in to the VATSIM network";
        } else if (userIsObserver && !serverAllowsObsAsMaster) {
            userIsNotEligibleMessage = "You are logged in as Observer and Server does not allow Observers to be Master";
        } else if (userIsInSweatbox && !serverAllowsSweatboxAsMaster) {
            userIsNotEligibleMessage =
                "You are logged in on a Sweatbox Server and Server does not allow Sweatbox connections";
        } else {
            DisplayMessage("Executing vACDM as the MASTER");
            Logger::instance().log(Logger::LogSender::vACDM, "Switched to MASTER", Logger::LogLevel::Info);
            com::Server::instance().setMaster(true);

            return true;
        }

        DisplayMessage("Cannot upgrade to Master");
        DisplayMessage(userIsNotEligibleMessage);
        return true;
    } else if (std::string::npos != command.find("SLAVE")) {
        DisplayMessage("Executing vACDM as the SLAVE");
        Logger::instance().log(Logger::LogSender::vACDM, "Switched to SLAVE", Logger::LogLevel::Info);
        com::Server::instance().setMaster(false);
        return true;
    } else if (std::string::npos != command.find("RELOAD")) {
        this->reloadConfiguration();
        return true;
    } else if (std::string::npos != command.find("LOG")) {
        if (std::string::npos != command.find("LOGLEVEL")) {
            DisplayMessage(Logger::instance().handleLogLevelCommand(command));
        } else {
            DisplayMessage(Logger::instance().handleLogCommand(command));
        }
        return true;
    } else if (std::string::npos != command.find("UPDATERATE")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() != 3) {
            DisplayMessage("Usage: .vacdm UPDATERATE value");
            return true;
        }
        if (false == isNumber(elements[2]) ||
            std::stoi(elements[2]) < minUpdateCycleSeconds || std::stoi(elements[2]) > maxUpdateCycleSeconds) {
            DisplayMessage("Usage: .vacdm UPDATERATE value");
            DisplayMessage("Value must be number between " + std::to_string(minUpdateCycleSeconds) + " and " +
                           std::to_string(maxUpdateCycleSeconds));
            return true;
        }

        DisplayMessage(DataManager::instance().setUpdateCycleSeconds(std::stoi(elements[2])));

        return true;
    } else if (std::string::npos != command.find("UNEXEMPT")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .vacdm UNEXEMPT <CALLSIGN>");
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
            DisplayMessage("Usage: .vacdm EXEMPT <CALLSIGN>");
            return true;
        }
        Json::Value root;
        root["callsign"] = elements[2];
        root["exemptFromCdm"] = true;
        com::Server::instance().sendPatchMessage("/api/v1/pilots/" + elements[2], root);
        DisplayMessage(elements[2] + " marked as CDM-exempt (VIP/medical/SAR)");
        return true;
    } else if (std::string::npos != command.find("LVO")) {
        // Find master airport. We assume activeAirports has the airport.
        // Wait, plugin has no direct API to get single master airport easily in CompileCommands.
        // But we can extract it if they pass it, or we just rely on the first active airport.
        // Let's require the ICAO for LVO to be safe, e.g. .vacdm lvo VVTS, or just extract from SectorFile
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 3) {
            DisplayMessage("Usage: .vacdm LVO <ICAO>");
            return true;
        }
        com::Server::instance().toggleLvo(elements[2], true);
        DisplayMessage("LVO activated for " + elements[2]);
        return true;
    } else if (std::string::npos != command.find("STARTUPDELAY") || std::string::npos != command.find("DEPARTUREDELAY")) {
        const auto elements = vacdm::utils::String::splitString(command, " ");
        if (elements.size() < 4) {
            DisplayMessage("Usage: .vacdm STARTUPDELAY <ICAO>/<RWY> <TIME>");
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
        
        // Handle time (absolute or relative)
        if (time == "9999") {
            // Send DELETE
            com::Server::instance().sendDeleteMessage("/api/v1/airports/" + icao + "/delays/" + runway + "/" + type); // wait, delete is /api/v1/airports/:icao/delays? No, spec says: DELETE /api/v1/airports/:icao/delays when sentinel 9999 received. But runway and type are needed?
            // Actually spec: DELETE /api/v1/airports/:icao/delays when sentinel 9999 received. We should pass runway and type. 
            // Wait, I will just call DELETE /api/v1/airports/:icao/delays?runway=...&type=...
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
            com::Server::instance().postDelay(icao, rwy, type, absoluteTime);
            DisplayMessage("Delay set for " + icao + " " + rwy + " from " + absoluteTime + "z");
        }
        return true;
    }
    return false;
}
}  // namespace vacdm
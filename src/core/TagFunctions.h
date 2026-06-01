#pragma once

#pragma warning(push, 0)
#include "EuroScopePlugIn.h"
#pragma warning(pop)

#include "core/DataManager.h"
#include "core/Server.h"
#include "types/Pilot.h"
#include "utils/Date.h"
#include "utils/Number.h"
#include "vACDM.h"

using namespace vacdm;
using namespace vacdm::core;
using namespace vacdm::com;

namespace vacdm {

enum itemFunction {
    EXOT_MODIFY = 1,
    EXOT_NEW_VALUE,
    TOBT_NOW,
    TOBT_MANUAL,
    TOBT_MANUAL_EDIT,
    TOBT_MENU,
    ASAT_NOW,
    ASAT_NOW_AND_STARTUP,
    STARTUP_REQUEST,
    TOBT_CONFIRM,
    OFFBLOCK_REQUEST,
    AOBT_NOW_AND_STATE,
    RESET_TOBT,
    RESET_ASAT,
    RESET_ASRT,
    RESET_TOBT_CONFIRM,
    RESET_AORT,
    RESET_AOBT_AND_STATE,
    RESET_MENU,
    RESET_PILOT,
    ARDT_NOW,
    ASRT_NOW,
    TSAC_NOW,
    TSAC_MANUAL,
    TSAC_MANUAL_EDIT,
    TSAC_MENU,
    TSAC_REMOVE,
};

void vACDM::RegisterTagItemFuntions() {
    RegisterTagItemFunction("Set ARDT", itemFunction::ARDT_NOW);
    RegisterTagItemFunction("Set ASRT", itemFunction::ASRT_NOW);
}

void vACDM::OnFunctionCall(int functionId, const char * /*itemString*/, POINT /*pt*/, RECT /*area*/) {
    if (functionId != itemFunction::ARDT_NOW && functionId != itemFunction::ASRT_NOW) return;

    const bool recordingArdt = functionId == itemFunction::ARDT_NOW;
    const std::string label = recordingArdt ? "ARDT" : "ASRT";

    auto flightplan = FlightPlanSelectASEL();
    if (!flightplan.IsValid()) {
        DisplayMessage("Unable to record " + label + ": no selected aircraft.", label);
        return;
    }

    const std::string callsign = flightplan.GetCallsign();
    if (false == DataManager::instance().checkPilotExists(callsign)) {
        DisplayMessage("Unable to record " + label + ": " + callsign + " is not tracked by VCLvACDM.", label);
        return;
    }

    const auto pilot = DataManager::instance().getPilot(callsign);
    const bool alreadySet = recordingArdt ? pilot.ardt != types::defaultTime : pilot.asrt != types::defaultTime;
    if (alreadySet) {
        DisplayMessage(label + " already set for " + callsign + ".", label);
        return;
    }
    if (!Server::instance().isMaster(pilot.origin)) {
        DisplayMessage("Unable to record " + label + ": not master for " + pilot.origin + ".", label);
        return;
    }

    const auto messageType = recordingArdt ? DataManager::MessageType::UpdateARDT : DataManager::MessageType::UpdateASRT;
    DataManager::instance().handleTagFunction(messageType, callsign,
                                              std::chrono::utc_clock::now());
    DisplayMessage(label + " recorded for " + callsign + ".", label);
}
}  // namespace vacdm

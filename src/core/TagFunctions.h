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
    TSAC_NOW,
    TSAC_MANUAL,
    TSAC_MANUAL_EDIT,
    TSAC_MENU,
    TSAC_REMOVE,
};

void vACDM::RegisterTagItemFuntions() {
    RegisterTagItemFunction("Set ARDT", itemFunction::ARDT_NOW);
}

void vACDM::OnFunctionCall(int functionId, const char * /*itemString*/, POINT /*pt*/, RECT /*area*/) {
    if (functionId != itemFunction::ARDT_NOW) return;

    auto flightplan = FlightPlanSelectASEL();
    if (!flightplan.IsValid()) {
        DisplayMessage("Unable to record ARDT: no selected aircraft.", "ARDT");
        return;
    }

    const std::string callsign = flightplan.GetCallsign();
    if (false == DataManager::instance().checkPilotExists(callsign)) {
        DisplayMessage("Unable to record ARDT: " + callsign + " is not tracked by VCLvACDM.", "ARDT");
        return;
    }

    const auto pilot = DataManager::instance().getPilot(callsign);
    if (pilot.ardt != types::defaultTime) {
        DisplayMessage("ARDT already set for " + callsign + ".", "ARDT");
        return;
    }
    if (!Server::instance().isMaster(pilot.origin)) {
        DisplayMessage("Unable to record ARDT: not master for " + pilot.origin + ".", "ARDT");
        return;
    }

    DataManager::instance().handleTagFunction(DataManager::MessageType::UpdateARDT, callsign,
                                              std::chrono::utc_clock::now());
    DisplayMessage("ARDT recorded for " + callsign + ".", "ARDT");
}
}  // namespace vacdm

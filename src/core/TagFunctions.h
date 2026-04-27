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
    TSAC_NOW,
    TSAC_MANUAL,
    TSAC_MANUAL_EDIT,
    TSAC_MENU,
    TSAC_REMOVE,
};

void vACDM::RegisterTagItemFuntions() {
    // Tag functions disabled to prevent conflict with GroundRadar plugin
}

void vACDM::OnFunctionCall(int functionId, const char *itemString, POINT pt, RECT area) {
    // Tag functions disabled to prevent conflict with GroundRadar plugin
}
}  // namespace vacdm
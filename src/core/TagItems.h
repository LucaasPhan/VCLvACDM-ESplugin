#pragma once

#include <wtypes.h>

#include <chrono>
#include <format>
#include <string>

#include "TagItemsColor.h"
#include "types/Pilot.h"
#include "vACDM.h"

using namespace vacdm::tagitems;

namespace vacdm {
enum itemType {
    EOBT,
    TOBT,
    TSAT,
    TTOT,
    EXOT,
    ASAT,
    AOBT,
    ATOT,
    ASRT,
    AORT,
    CTOT,
    FLTTYPE,
    GROUND_HANDLER,
    EXEMPT,
    EVENT_BOOKING,
    TSAC,
    TOBT_SET_BY,
    E_STATUS,
    STATUS,
};

void vACDM::RegisterTagItemTypes() {
    RegisterTagItemType("EOBT", itemType::EOBT);
    RegisterTagItemType("TOBT", itemType::TOBT);
    RegisterTagItemType("TSAT", itemType::TSAT);
    RegisterTagItemType("TTOT", itemType::TTOT);
    RegisterTagItemType("EXOT", itemType::EXOT);
    RegisterTagItemType("ASAT", itemType::ASAT);
    RegisterTagItemType("AOBT", itemType::AOBT);
    RegisterTagItemType("ATOT", itemType::ATOT);
    RegisterTagItemType("ASRT", itemType::ASRT);
    RegisterTagItemType("AORT", itemType::AORT);
    RegisterTagItemType("CTOT", itemType::CTOT);
    RegisterTagItemType("FLT TYPE", itemType::FLTTYPE);
    RegisterTagItemType("GND HDL", itemType::GROUND_HANDLER);
    RegisterTagItemType("EXEMPT", itemType::EXEMPT);
    RegisterTagItemType("Event Booking", itemType::EVENT_BOOKING);
    RegisterTagItemType("TSAC", itemType::TSAC);
    RegisterTagItemType("TOBT-SET-BY", itemType::TOBT_SET_BY);
    RegisterTagItemType("E", itemType::E_STATUS);
    RegisterTagItemType("STATUS", itemType::STATUS);
}

std::string formatTime(const std::chrono::utc_clock::time_point timepoint) {
    if (timepoint.time_since_epoch().count() > 0)
        return std::format("{:%H%M}", timepoint);
    else
        return "";
}

void vACDM::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget,
                         int ItemCode, int TagData, char sItemString[16], int *pColorCode, COLORREF *pRGB,
                         double *pFontSize) {
    std::ignore = RadarTarget;
    std::ignore = TagData;
    std::ignore = pRGB;
    std::ignore = pFontSize;

    *pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
    if (nullptr == FlightPlan.GetFlightPlanData().GetPlanType() ||
        0 == std::strlen(FlightPlan.GetFlightPlanData().GetPlanType()))
        return;
    // skip non IFR flights
    if (std::string_view("I") != FlightPlan.GetFlightPlanData().GetPlanType()) {
        return;
    }
    std::string callsign = FlightPlan.GetCallsign();

    if (false == DataManager::instance().checkPilotExists(callsign)) return;

    auto pilot = DataManager::instance().getPilot(callsign);

    std::stringstream outputText;
    const bool exempt = pilot.exemptFromCdm;

    if (exempt && (ItemCode == static_cast<int>(itemType::TOBT) || ItemCode == static_cast<int>(itemType::TSAT) ||
                   ItemCode == static_cast<int>(itemType::TTOT) || ItemCode == static_cast<int>(itemType::CTOT))) {
        outputText << "----";
        *pRGB = Color::pluginConfig.grey;
        std::strcpy(sItemString, outputText.str().c_str());
        return;
    }

    std::string eStatus = "C";
    const auto now = std::chrono::utc_clock::now();
    if (pilot.eobt != types::defaultTime && pilot.eobt > now + std::chrono::minutes(35)) {
        eStatus = "P";
    } else if (pilot.tsat != types::defaultTime && pilot.tsat + std::chrono::minutes(6) < now && pilot.asat == types::defaultTime) {
        eStatus = "I";
    }

    switch (static_cast<itemType>(ItemCode)) {
        case itemType::EOBT:
            outputText << formatTime(pilot.eobt);
            *pRGB = Color::colorizeEobt(pilot);
            break;
        case itemType::TOBT:
            outputText << formatTime(pilot.tobt);
            *pRGB = Color::colorizeTobt(pilot);
            break;
        case itemType::TSAT:
            if (eStatus == "P") {
                outputText << "~~";
                *pRGB = Color::pluginConfig.darkgreen;
            } else {
                outputText << formatTime(pilot.tsat);
                *pRGB = Color::colorizeTsat(pilot);
                if (pilot.ctot != types::defaultTime && pilot.ttot != types::defaultTime) {
                    const auto diffSeconds =
                        std::abs(std::chrono::duration_cast<std::chrono::seconds>(pilot.ttot - pilot.ctot).count());
                    if (diffSeconds > 300) *pRGB = Color::pluginConfig.red;
                }
            }
            break;
        case itemType::TTOT:
            if (eStatus == "P") {
                outputText << "~~";
                *pRGB = Color::pluginConfig.darkgreen;
            } else {
                outputText << formatTime(pilot.ttot);
                *pRGB = Color::colorizeTtot(pilot);
            }
            break;
        case itemType::EXOT:
            if (pilot.exot.time_since_epoch().count() > 0) {
                outputText << std::format("{:%M}", pilot.exot);
                *pColorCode = Color::colorizeExot(pilot);
            }
            break;
        case itemType::ASAT:
            outputText << formatTime(pilot.asat);
            *pRGB = Color::colorizeAsat(pilot);
            break;
        case itemType::AOBT:
            outputText << formatTime(pilot.aobt);
            *pRGB = Color::colorizeAobt(pilot);
            break;
        case itemType::ATOT:
            outputText << formatTime(pilot.atot);
            *pRGB = Color::colorizeAtot(pilot);
            break;
        case itemType::ASRT:
            outputText << formatTime(pilot.asrt);
            *pRGB = Color::colorizeAsrt(pilot);
            break;
        case itemType::AORT:
            outputText << formatTime(pilot.aort);
            *pRGB = Color::colorizeAort(pilot);
            break;
        case itemType::CTOT:
            outputText << formatTime(pilot.ctot);
            *pRGB = (pilot.ctot == types::defaultTime) ? Color::pluginConfig.grey : Color::pluginConfig.orange;
            break;
        case itemType::FLTTYPE:
            outputText << (pilot.flightType == "DOMESTIC" ? "DOM" : "INT");
            *pRGB = Color::pluginConfig.grey;
            break;
        case itemType::GROUND_HANDLER:
            outputText << pilot.groundHandler;
            *pRGB = Color::pluginConfig.grey;
            break;
        case itemType::EXEMPT:
            outputText << (exempt ? "LOCK" : "");
            *pRGB = Color::pluginConfig.grey;
            break;
        case itemType::EVENT_BOOKING:
            outputText << (pilot.hasBooking ? "B" : "");
            *pRGB = Color::colorizeEventBooking(pilot);
            break;
        case itemType::TSAC:
            outputText << pilot.tsac;
            if (!pilot.tsac.empty() && pilot.tsat != types::defaultTime) {
                const auto tsacTime = utils::Date::convertStringToTimePoint(pilot.tsac);
                const auto diffMins = std::abs(std::chrono::duration_cast<std::chrono::minutes>(pilot.tsat - tsacTime).count());
                *pRGB = (diffMins <= 5) ? Color::pluginConfig.darkgreen : Color::pluginConfig.orange;
            } else {
                *pRGB = Color::pluginConfig.orange;
            }
            break;
        case itemType::TOBT_SET_BY:
            outputText << pilot.tobtSetBy;
            *pRGB = Color::pluginConfig.darkgreen;
            break;
        case itemType::E_STATUS:
            outputText << eStatus;
            *pRGB = Color::pluginConfig.darkgreen;
            break;
        case itemType::STATUS:
            if (pilot.ctot != types::defaultTime) {
                outputText << "CTO";
                *pRGB = Color::pluginConfig.orange;
            } else if (pilot.tsatReset) {
                outputText << "RES";
                *pRGB = Color::pluginConfig.red;
            } else if (eStatus == "I") {
                outputText << "PAS";
                *pRGB = Color::pluginConfig.orange;
            } else if (pilot.asat != types::defaultTime) {
                outputText << "STR"; // Started
                *pRGB = Color::pluginConfig.darkgreen;
            } else if (pilot.ardt != types::defaultTime) {
                outputText << "RDY";
                *pRGB = Color::pluginConfig.darkgreen;
            } else if (pilot.tsat != types::defaultTime) {
                outputText << "SEQ";
                *pRGB = Color::pluginConfig.darkgreen;
            }
            break;
        default:
            break;
    }

    std::strcpy(sItemString, outputText.str().c_str());
}
}  // namespace vacdm
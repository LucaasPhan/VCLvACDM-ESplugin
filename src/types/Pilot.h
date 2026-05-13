#pragma once

#include <chrono>
#include <string>
#include <vector>


namespace vacdm::types {
static constexpr std::chrono::utc_clock::time_point defaultTime =
    std::chrono::utc_clock::time_point(std::chrono::milliseconds(-1));

typedef struct Pilot_t {
    std::string callsign;
    std::string cid;
    std::chrono::utc_clock::time_point lastUpdate = defaultTime;

    bool inactive = false;

    // position data

    double latitude = 0.0;
    double longitude = 0.0;
    bool taxizoneIsTaxiout = false;

    // flightplan & clearance data

    std::string origin;
    std::string destination;
    std::string runway;
    std::string sid;
    std::string aircraft;
    std::string flightType;
    std::string airline;
    std::string groundHandler;
    bool exemptFromCdm = false;
    std::string groundState;

    // ACDM procedure data

    std::chrono::utc_clock::time_point eobt = defaultTime;
    std::chrono::utc_clock::time_point tobt = defaultTime;
    std::string tobt_state;
    std::chrono::utc_clock::time_point ctot = defaultTime;
    std::chrono::utc_clock::time_point ttot = defaultTime;
    std::chrono::utc_clock::time_point tsat = defaultTime;
    std::chrono::utc_clock::time_point exot = defaultTime;
    std::chrono::utc_clock::time_point asat = defaultTime;
    std::chrono::utc_clock::time_point aobt = defaultTime;
    std::chrono::utc_clock::time_point atot = defaultTime;
    std::chrono::utc_clock::time_point asrt = defaultTime;
    std::chrono::utc_clock::time_point ardt = defaultTime;
    std::chrono::utc_clock::time_point aort = defaultTime;
    std::string tsac;
    std::string tobtSetBy;
    bool tsatReset = false;


    // event booking data

    bool hasBooking = false;
} Pilot;
}  // namespace vacdm::types
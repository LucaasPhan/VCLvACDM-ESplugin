# VCLvACDM EuroScope Plugin

Windows EuroScope DLL plugin for VCL vACC that syncs departures with the VCLvACDM backend every 5 seconds.

## Build

- Toolchain: CMake + Conan + MSVC
- Output DLL target: `VCLvACDM.dll`
- Config file: `vacdm.txt`

## Controller Setup

1. Copy `vacdm.txt.template` to `vacdm.txt`.
2. Set `SERVER_url` to your VCLvACDM backend.
3. Place `VCLvACDM.dll` and `vacdm.txt` in the EuroScope plugin directory.
4. Load plugin in EuroScope.

## Dot Commands

### Core Management
- `.vacdm master <ICAO>` — Claim mastership for a specific airport. Only the Master can push updates.
- `.vacdm slave <ICAO>` — Release mastership and return to read-only mode for that airport.
- `.vacdm reload` — Reload settings from `vacdm.txt` without restarting EuroScope.
- `.vacdm updaterate <seconds>` — Set the poll interval (e.g., `.vacdm updaterate 5`). Valid range: 1–30s.

### Flight & Sequence Control
- `.vacdm exempt <callsign>` — Mark a flight as CDM-exempt (VIP, SAR, Medical). Renders as `----` in sequence.
- `.vacdm unexempt <callsign>` — Remove exempt status and return flight to the sequence.
- `.vacdm lvo <ICAO>` — Activate Low Visibility Operations (LVO) rate for an airport.

### Delay Restrictions
- `.vacdm startupdelay <ICAO>/<RWY> <TIME>` — Shift TSATs for a specific runway.
- `.vacdm departuredelay <ICAO>/<RWY> <TIME>` — Shift TTOTs for a specific runway.
    - **Absolute Time**: Use 4 digits (e.g., `1230`).
    - **Relative Time**: Use 1–2 digits for minutes from now (e.g., `15` for now + 15m).
    - **Clear**: Use `9999` to remove the delay restriction.

### Logging
- `.vacdm log on|off|debug` — Toggle plugin logging.
- `.vacdm loglevel <sender> <level>` — Set logging level per module.

## Vietnam-specific behavior

- New startup list items: `CTOT`, `FLT TYPE`, `GND HDL`, `EXEMPT`, `STATUS`
- **`STATUS` Tag Codes**:
    - `CTO` (Orange): CTOT assigned (Network Slot).
    - `RES` (Red): TSAT has been reset (TOBT update or manual reset).
    - `PAS` (Yellow): Overdue for startup (>6 mins past TSAT).
    - `RDY` (Green): Pilot reported ready.
    - `STR` (Green): Startup approved (ASAT recorded).
    - `SEQ` (Green): Assigned and in sequence.
- `EXEMPT` flights show `LOCK`; TOBT/TSAT/TTOT/CTOT render as `----`
- TSAT highlights CTOT violation when TTOT is outside CTOT +/- 5 minutes
- Airports with `acdmStatus` of `PRE_CDM` or `INACTIVE` are read-only (display only, no POST/PATCH writes)
- **Master/Slave Panel**: A status panel in the top-left of the radar screen shows management status per airport.

## Backend Contract Notes

Plugin expects:

- `GET /api/v1/health`
- `GET /api/v1/pilots?airport=ICAO`
- `POST /api/v1/pilots`
- `PATCH /api/v1/pilots/:callsign`
- `DELETE /api/v1/pilots/:callsign`
- `GET /api/v1/airports/:icao`
- `GET /api/v1/airports/:icao/blocks`

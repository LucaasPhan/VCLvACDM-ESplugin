# VCLvACDM EuroScope Plugin

Windows EuroScope DLL plugin for VCL vACC that syncs departures with the VCLvACDM backend every 5 seconds.

## Build

- Toolchain: CMake + Conan + MSVC
- Output DLL target: `VCLvACDM.dll`
- Config file: `vacdm.txt`

## Controller Setup
1. Place `VCLvACDM` folder after extraction in the sector plugin directory.
2. Load plugin in EuroScope.

## Dot Commands
All commands use the `.acdm` prefix.

### Core Management
- `.acdm master <ICAO>` — Claim mastership for an airport. Only the Master can push updates. **Currently restricted to VVTS and VVNB.**
- `.acdm slave` — Release all local mastership claims.
- `.acdm slave <ICAO>` — Release mastership for a specific airport.
- `.acdm reload` — Reload settings from `vacdm.txt` without restarting EuroScope.
- `.acdm debug on|off` — Toggle Debug Mode (allows polling and commands while disconnected).

### Flight & Sequence Control
- `.acdm exempt <callsign>` — Mark a flight as CDM-exempt (VIP, SAR, Medical). Renders as `----` in sequence.
- `.acdm unexempt <callsign>` — Remove exempt status and return flight to the sequence.
- `.acdm lvo <ICAO> on|off` — Toggle Low Visibility Operations (LVO) for an airport (VVTS/VVNB only).

### Delay Restrictions
- `.acdm startupdelay <ICAO>/<RWY> <TIME>` — Shift TSATs for a specific runway.
- `.acdm departuredelay <ICAO>/<RWY> <TIME>` — Shift TTOTs for a specific runway.
    - **Absolute Time**: Use 4 digits (e.g., `1230`).
    - **Relative Time**: Use 1–2 digits for minutes from now (e.g., `15` for now + 15m).
    - **Clear**: Use `9999` to remove the delay restriction.

### Logging
- `.acdm log on|off|debug` — Toggle plugin logging.
- `.acdm loglevel <sender> <level>` — Set logging level per module.

## Vietnam-specific behavior

- New startup list items: `CTOT`, `FLT TYPE`, `GND HDL`, `EXEMPT`, `STATUS`
- **`STATUS` Tag Codes**:
    - `CTO` (Orange): CTOT assigned (Network Slot).
    - `RES` (Red): TSAT has been reset (TOBT update or manual reset).
    - `PAS` (Orange): Overdue for startup (>6 mins past TSAT).
    - `RDY` (Green): Pilot reported ready.
    - `STR` (Green): Startup approved (ASAT recorded).
    - `SEQ` (Green): Assigned and in sequence.
- `EXEMPT` flights show `LOCK`; TOBT/TSAT/TTOT/CTOT render as `----`
- TSAT highlights CTOT violation when TTOT is outside CTOT +/- 5 minutes
- **Master/Slave Panel**: A status panel in the top-left of the radar screen shows management status for VVTS and VVNB.
    - **MASTER** (Amber): You are pushing data for this airport.
    - **SLAVE** (Blue): Another controller is master, or the airport is unmanaged.

## Backend Contract Notes

Plugin expects:

- `GET /api/v1/health`
- `GET /api/v1/pilots?airport=ICAO`
- `POST /api/v1/pilots`
- `PATCH /api/v1/pilots/:callsign`
- `DELETE /api/v1/pilots/:callsign`
- `GET /api/v1/airports/:icao`
- `GET /api/v1/airports/:icao/blocks`

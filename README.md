# VCLvACDM EuroScope Plugin

Windows EuroScope DLL plugin for VCL vACC that syncs departures with the VCLvACDM backend every 5 seconds.

## Build

- Toolchain: CMake + Conan + MSVC
- Output DLL target: `VCLvACDM.dll`
- Config file copied to build output: `vacdm.txt`

## Controller Setup

1. Copy `vacdm.txt.template` to `vacdm.txt`.
2. Set `SERVER_url` to your VCLvACDM backend.
3. Place `VCLvACDM.dll` and `vacdm.txt` in the EuroScope plugin directory.
4. Load plugin in EuroScope.

Default backend URL:

- `https://api.vclacdm.vclvacc.net`

## Dot Commands

- `.vacdm master` - enable write mode
- `.vacdm slave` - enable read-only mode
- `.vacdm reload` - reload `vacdm.txt`
- `.vacdm log on|off|debug` - logging controls
- `.vacdm loglevel <sender> <level>` - per-sender logging level
- `.vacdm updaterate <1-10>` - backend poll interval
- `.vacdm exempt <callsign>` - set `exemptFromCdm=true`
- `.vacdm unexempt <callsign>` - set `exemptFromCdm=false`

## Vietnam-specific behavior

- New startup list items: `CTOT`, `FLT TYPE`, `GND HDL`, `EXEMPT`
- `EXEMPT` flights show `LOCK`; TOBT/TSAT/TTOT/CTOT render as `----`
- TSAT highlights CTOT violation when TTOT is outside CTOT +/- 5 minutes
- Airports with `acdmStatus` of `PRE_CDM` or `INACTIVE` are read-only (display only, no POST/PATCH writes)

## Backend Contract Notes

Plugin expects:

- `GET /api/v1/health`
- `GET /api/v1/pilots?airport=ICAO`
- `POST /api/v1/pilots`
- `PATCH /api/v1/pilots/:callsign`
- `DELETE /api/v1/pilots/:callsign`
- `GET /api/v1/airports/:icao`
- `GET /api/v1/airports/:icao/blocks`

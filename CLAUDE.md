# CLAUDE.md — VCLvACDM Plugin (C++ EuroScope DLL)
 
## Project
VCLvACDM Plugin — forked from `vACDM/vacdm-plugin`. A C++ Windows DLL for EuroScope that connects to the VCLvACDM NestJS backend for VCL vACC Vietnam A-CDM operations on VATSIM.
 
## Stack
- **Language**: C++17, MSVC (Windows only — EuroScope is Windows-only)
- **Build**: CMake + Conan
- **HTTP**: libcurl (async, never block EuroScope thread)
- **JSON**: nlohmann/json
- **EuroScope SDK**: EuroScope plug-in API (bundled in `/EuroScope/`)
- **Output**: `VCLvACDM.dll` — loaded by EuroScope at runtime
 
## Build Commands
```bash
# Install dependencies
conan install . --build=missing -s build_type=Release
 
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release
 
# Build
cmake --build build --config Release
 
# Output DLL location
build/Release/VCLvACDM.dll
```
 
## Key Architecture
 
### HTTP Communication
All HTTP calls are **asynchronous** — they must never block the EuroScope main thread. Follow the existing pattern: queue requests to a background thread, process results when EuroScope calls back into the plugin.
 
The plugin polls the backend every 5 seconds:
```
POST   /api/v1/pilots              ← upsert pilot from EuroScope data
GET    /api/v1/pilots?airport=ICAO ← read back TSAT/TTOT computed by server
PATCH  /api/v1/pilots/:callsign    ← ATC edits (TOBT, ASRT, ASAT)
DELETE /api/v1/pilots/:callsign    ← aircraft disconnect
GET    /api/v1/airports/:icao      ← load taxizone/capacity config on startup
GET    /api/v1/airports/:icao/blocks ← capacity block data
GET    /api/v1/health              ← backend connectivity check
```
 
### Config File (`vacdm.txt`)
Loaded from same directory as the DLL. Key settings:
- `SERVER_url` — backend base URL (no trailing slash)
- `API_KEY` — backend API key for authentication
### Pilot Data Flow
EuroScope → plugin reads flight plan + radar target → POST to backend → backend calculates TSAT/TTOT → GET back → plugin renders in EuroScope startup list columns
 
### Master/Slave Mode
- `.acdm master` — enables write mode (TOBT edits, ASRT/ASAT marking)
- `.acdm slave` — read-only mode (default)
- Only master can send PATCH requests
## Vietnam-Specific Fields
These fields are added by this fork (not in original vACDM plugin):
 
| Field | Type | Source | Notes |
|-------|------|--------|-------|
| `flightType` | `"DOMESTIC"\|"INTERNATIONAL"` | Derived from ADEP/ADES | Both VV* prefix = DOM |
| `airline` | string | First 3 chars of callsign | HVN, VJC, BAV, PIC, VVA |
| `groundHandler` | string\|null | ATC-set via frontend | SAGS, VIAGS, NASCO |
| `exemptFromCdm` | boolean | ATC-set | VIP, medical, SAR flights |
| `ctot` | ISO datetime\|null | Backend (ATFMC VN) | Displayed orange in CTOT column |
 
## Tag Item IDs
| ID | Name | Notes |
|----|------|-------|
| existing | TOBT | |
| existing | TSAT | Colour: dark green=window, orange=missed |
| existing | TTOT | |
| existing | ASRT | |
| existing | ASAT | |
| 20 | CTOT | Orange text when set |
| 21 | FLT TYPE | DOM or INT |
| 22 | GND HDL | Ground handler code |
| 23 | EXEMPT | 🔒 if exempt |
 
## Vietnam Airports
| ICAO | Status | Plugin behaviour |
|------|--------|-----------------|
| VVTS | FULL | Full read/write |
| VVNB | PHASE1 | Full read/write |
| VVDN | PRE_CDM | Read-only display only |
 
## Dot Commands
```
.acdm master                  → enable write mode
.acdm slave                   → enable read-only mode
.acdm reload                  → reload vacdm.txt
.acdm log on/off/debug        → logging control
.acdm exempt <callsign>       → mark flight CDM-exempt (VIP/SAR)
.acdm unexempt <callsign>     → remove CDM-exempt flag
```
 
## Critical Rules
- **Never block EuroScope thread** — all HTTP async
- **CORS not applicable** — plugin sends requests from local machine, no browser CORS
- **Windows only** — no POSIX, no Linux headers
- **nlohmann/json only** — no additional JSON libraries
- **DLL name is `VCLvACDM.dll`** — not `vACDM.dll`
- **Do not modify ECFMP integration** — disabled for VCL vACC (uses ATFMC VN instead)
## Backend Contract (what the plugin expects)
See `API_CONTRACT.md` for full request/response shapes. Key rule: field names in JSON responses must match **exactly** — the plugin hardcodes string keys.
 
Required backend endpoints (all under `/api/v1/`):
- `POST /pilots` — upsert
- `GET /pilots?airport=ICAO` — list
- `PATCH /pilots/:callsign` — partial update
- `DELETE /pilots/:callsign` — remove
- `GET /airports/:icao` — config (fields: `standard_taxitime`, `taxizones`, `capacities`, `acdmStatus`)
- `GET /airports/:icao/blocks` — capacity blocks
- `GET /health` — connectivity check
# AGENTS.md — Agentic Coding Guide for VCLvACDM Plugin

Rules for AI coding agents (Claude Code, Cursor, Gemini CLI, Copilot) operating on this C++ EuroScope plugin repo.

## Repo Identity
VCLvACDM Plugin — forked C++ EuroScope DLL, adapted for VCL vACC Vietnam. Connects to VCLvACDM NestJS backend at `vacdm-backend/`.

## Permitted Autonomous Actions
- Read any file in this repo
- Edit `.cpp`, `.h`, `.cmake`, `.txt`, `.md` files
- Add new source files under `src/`
- Run `cmake --build build --config Release` to verify compilation
- Run existing tests if present

## Requires Human Confirmation Before Running
- Any change to `CMakeLists.txt` that adds or removes dependencies
- Changes to the EuroScope SDK interface files in `/EuroScope/`
- Changes to HTTP request functions (async threading is fragile — risk of EuroScope crash)
- Any `git push` — human reviews before publish
- Changing the plugin name string (affects EuroScope plugin registry)

## Never Do
- Block the EuroScope thread — all HTTP must be async (existing background thread pattern)
- Use POSIX headers (`unistd.h`, `pthread.h`) — Windows DLL only
- Add a second JSON library — `nlohmann/json` is already present, use it
- Add a second HTTP library — `libcurl` is already wired, use it
- Modify files in `/EuroScope/` SDK — read-only reference, never edit
- Use `std::cout` for output — use the EuroScope `DisplayUserMessage` function
- Output a DLL named anything other than `VCLvACDM.dll`
- Use C++20 features — target C++17 for MSVC compatibility

## Code Conventions
- **Naming**: follow existing plugin conventions — CamelCase classes, camelCase methods, UPPER_SNAKE for constants
- **Error handling**: all HTTP response codes checked; on non-2xx, log via `DisplayUserMessage` and degrade gracefully
- **JSON parsing**: always use `.value("key", default)` not `["key"]` to avoid exceptions on missing fields
- **Threading**: new data goes into a thread-safe queue; EuroScope callback processes the queue on main thread
- **Logging**: prefix all messages with `[VCLvACDM]` and optionally `[ICAO]`

## Reading the Codebase
Before modifying any file, always read:
1. The file you plan to edit
2. Its corresponding header if it's a `.cpp`
3. Any file that calls functions you plan to change

EuroScope plugins are event-driven. The main entry points are:
- `OnRadarScreenCreated()` — plugin load
- `OnFlightPlanFlightPlanDataUpdate()` — flight plan change
- `OnFlightPlanDisconnect()` — aircraft disconnect
- `OnGetTagItem()` — render a startup list column
- `OnFunctionCall()` — ATC clicks a column to edit
- `OnTimer()` — periodic tick (drives the HTTP update loop)
- `OnCompileCommand()` — handles `.acdm` dot commands

## API Contract (memorise this)
The plugin sends/receives JSON to/from the NestJS backend. Field names are hardcoded strings. Any mismatch = silent data loss. The exact contract is in `CLAUDE.md` and `API_CONTRACT.md`. When in doubt, check those files — do not guess field names.

## Vietnam-Specific Logic
- `flightType`: `"DOMESTIC"` if both ADEP and ADES start with `VV`, else `"INTERNATIONAL"`
- `acdmStatus == "PRE_CDM"` or `"INACTIVE"`: skip all POST/PATCH for that airport
- `exemptFromCdm == true`: render all time columns as `----` in grey
- `ctot` set + TTOT violates CTOT ± 5min: render TSAT in red

## PR / Completion Checklist
Before marking any task complete:
- [ ] Plugin compiles without errors (`cmake --build build --config Release`)
- [ ] No new warnings introduced
- [ ] All new `.acdm` commands documented in `README.md`
- [ ] No blocking HTTP calls on EuroScope main thread
- [ ] `VCLvACDM.dll` produced (not `vACDM.dll`)
- [ ] `vacdm.txt.template` updated if new config keys added
- [ ] Backend-side changes noted for the NestJS team if any API contract changes were required

## Companion Repos
This plugin repo works with two companion repos:
- `vacdm-backend/` — NestJS API, must expose the endpoints listed in `CLAUDE.md`
- `vacdm-frontend/` — Next.js dashboard, not relevant to plugin work

When making changes that affect the API contract (adding/changing fields), always note what the backend team needs to update.
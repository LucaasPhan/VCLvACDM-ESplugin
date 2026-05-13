# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0rc1] - 2026-05-13
### Added
- **Immediate Server Sync**: ACDM status changes (confirmed TOBT, ASRT, etc.) now trigger an immediate high-priority backend update.
- **Automated EOBT Sync**: Flight plans with future EOBTs are now automatically synchronized to the backend as `FLIGHTPLAN` TOBTs.
- **Improved Auto-Recording**: Optimized ASAT/AOBT/ATOT recording logic to ensure immediate data synchronization when ground states change.

### Fixed
- **SDK Method Errors**: Resolved `GetLatitude` and `GetLongitude` compilation errors in `CompileCommands.h`.
- **Warning Cleanup**: Eliminated numerous `C4100` (unreferenced parameter) warnings across the codebase for a cleaner build.

### Changed
- **Ground State Optimization**: Removed experimental "READY" ground-state set logic to favor manual controller flow.

---

## [1.3.0-beta1] - 2026-05-12
### Added
- **Aircraft Position Sync**: Real-time position tracking sent to backend for ground aircraft only.
- **Pilot Purging Mechanism**: Automatically removes local pilot data if the backend reports they are no longer active, ensuring cache consistency.
- **Improved Flight Type Detection**: Automated labeling of `DOMESTIC` vs `INTERNATIONAL` flights based on ADEP/ADES.
- **New Startup List Columns**: Added `CTOT`, `FLT TYPE`, `GND HDL`, `EXEMPT`, and `STATUS` items for EuroScope tags.
- **STATUS Tag System**: New visual indicators for flight states (`CTO`, `RES`, `PAS`, `RDY`, `STR`, `SEQ`).
- **CTOT Monitoring**: TSAT items now highlight in red when the estimated TTOT violates CTOT ± 5 minutes.
- **CDM Exemption**: New command `.acdm exempt <callsign>` to mark flights as exempt from CDM (e.g., VIP, SAR).

### Fixed
- **EuroScope SDK Compatibility**: Resolved `GetOnGround` and `GetPosition` compilation errors with latest EuroScope headers.
- **StatusPanel Mouse Events**: Fixed C3668 compilation errors in `StatusPanel` UI component.
- **SID Data Extraction**: Fixed issue where SIDs were not correctly pulled from EuroScope flight plans.
- **Clearance Status Logic**: Implemented automatic reset of clearance statuses when flight plans are modified.

### Changed
- **Async HTTP Engine**: Refined background threading to ensure zero blocking on the EuroScope main thread.
- **Robust JSON Parsing**: Switched to default-value parsing for all backend responses to prevent crashes on missing fields.
- **Code Standards**: Consolidated project settings to target C++17 for maximum MSVC compatibility.

---

## [1.2.0] - 2026-05-11
### Added
- **Rebranding**: Complete migration from `vACDM` to `VCLvACDM` branding across all messages and internal strings.
- **Enhanced Status Panel**: 
  - **LVO Integration**: Real-time display of Low Visibility Operations status.
  - **Runway Delays**: Active flow restrictions displayed directly in the panel.
- **Operational Commands**:
  - `.acdm DEBUG ON/OFF`: Allows testing while disconnected from the network.
  - `.acdm STARTUPDELAY` / `.acdm DEPARTUREDELAY`: Tools for flow control.
- **ISO Time Sync**: Ensured all delay times are sent in ISO 8601 format to prevent backend errors.

---

## [1.1.0] - 2026-05-08
### Added
- Core vACDM functionality for VCL vACC.
- Support for VVTS and VVNB airports.
- Master/Slave claim system.

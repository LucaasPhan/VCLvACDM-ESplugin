# RELEASE.md — VCLvACDM Plugin Changelog

## [1.2.0] — 2026-05-11
### Added
- **Rebranding**: Complete migration from `vACDM` to `VCLvACDM` branding across all messages and internal strings.
- **Enhanced Status Panel**:
  - **LVO Integration**: Real-time display of Low Visibility Operations status. Airport names now show `(LVO)` when active.
  - **Runway Delays**: Active flow restrictions (Startup/Departure delays) are now displayed directly in the panel with runway and time information.
  - **Dynamic Layout**: The panel height now adjusts automatically based on the number of active airports and delays.
- **Operational Commands**:
  - `.acdm DEBUG ON/OFF`: Allows polling and command usage while disconnected from the VATSIM network (useful for testing).
  - `.acdm LVO <ICAO> ON/OFF`: Toggle LVO status for supported airports (VVTS/VVNB).
  - `.acdm STARTUPDELAY <ICAO>/<RWY> <TIME>`: Set startup flow restrictions.
  - `.acdm DEPARTUREDELAY <ICAO>/<RWY> <TIME>`: Set departure flow restrictions.
- **System Improvements**:
  - **Auto-Reconnect**: Plugin now automatically re-detects active airports and resumes polling when reconnecting to the network.
  - **ISO Time Sync**: Fixed a backend 500 error by ensuring all delay times are sent in ISO 8601 format.

### Changed
- Improved error handling for HTTP requests to prevent crashes when aircraft data is missing.
- Refined logging to handle system-level requests without aircraft callsigns safely.

---

## [1.1.0] — Previous
### Added
- Core vACDM functionality for VCL vACC.
- Support for VVTS and VVNB airports.
- Master/Slave claim system.

---

## WIP / Roadmap
- [ ] **Dynamic LVO Flag**: Transition from hardcoded airport list to backend-driven `supportsLvo` flag when API is ready.
- [ ] **Tag Item Interactivity**: Implement direct clicks on radar tags to set markers/delays.
- [ ] **Scrollable Panel**: Add scrolling support to the Status Panel if the list of delays exceeds screen height.
- [ ] **Enhanced Flight Type Detection**: Automate detection for specific airline subgroups.

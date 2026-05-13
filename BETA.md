# VCLvACDM 1.3.0rc1 — Release Candidate Testing Guide

Welcome to the Beta release of the VCLvACDM EuroScope plugin. This version introduces significant architectural changes and new operational features specifically tailored for VCL vACC (Vietnam).

## Key Beta Features

### 1. Advanced Status Tracking
We have expanded the EuroScope Startup List with several new columns to provide a better overview of the sequence:
- **CTOT**: Displays Network Slots (Orange).
- **STATUS**: Real-time flight state (`RDY`, `STR`, `SEQ`, `PAS`, `RES`, `CTO`).
- **FLT TYPE**: Automatic detection of `DOM` vs `INT`.
- **GND HDL**: Ground Handler code (synced from backend).
- **EXEMPT**: Shows a 🔒 icon for CDM-exempt flights.

### 2. Live Position Synchronization
The plugin now synchronizes aircraft positions with the VCLvACDM backend for aircraft on the ground. This allows the backend to accurately track taxi times and runway occupancy without manual controller input.

### 3. Automated Logic
- **CTOT Monitoring**: The TSAT will automatically turn **RED** if the predicted TTOT violates the assigned CTOT window (± 5 minutes).
- **Auto-Reset**: Clearance statuses and TOBTs now automatically reset or update when significant flight plan changes are detected.
- **Pilot Purging**: Stale aircraft data is automatically removed when a pilot disconnects or is no longer tracked by the backend.

---

## How to Test

1. **Installation**:
   - Replace your existing `VCLvACDM.dll` with the beta version.
   - Ensure your `vacdm.txt` includes the correct `SERVER_url` and `API_KEY`.
2. **Setup**:
   - Add the new columns (`CTOT`, `FLT TYPE`, `STATUS`, `GND HDL`, `EXEMPT`) to your Departure/Startup list in EuroScope.
3. **Mastership**:
   - Use `.acdm master <ICAO>` to claim mastership for VVTS or VVNB.
4. **Commands**:
   - Test the new `.acdm exempt <callsign>` and `.acdm unexempt <callsign>` commands.

---

## Known Issues & Limitations

- **LVO Flags**: Currently uses a hardcoded list for VVTS/VVNB. Dynamic LVO support via backend is coming soon.
- **Tag Interactivity**: Direct clicking on tag items to set delays is still under development; please use dot commands for now.
- **Panel Scaling**: On very small resolutions, the Status Panel may overlap with other UI elements.

---

## Providing Feedback

Your feedback is critical for a stable v1.3.0 release. Please report issues via the following channels:
1. **Discord**: Post in the `#atc-tech-support` channel.
2. **GitHub**: Open an issue in the `vacdm-plugin` repository.

**When reporting a bug, please include:**
- Your EuroScope version.
- The specific airport (VVTS/VVNB).
- The `[VCLvACDM]` log output from the EuroScope message window.
- Steps to reproduce the issue.

Thank you for helping us improve VCL vACC operations!

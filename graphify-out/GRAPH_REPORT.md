# Graph Report - /Users/nhath/Documents/Code/VCLvACDM/vacdm-plugin  (2026-04-21)

## Corpus Check
- 25 files · ~38,468 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 109 nodes · 159 edges · 24 communities detected
- Extraction: 74% EXTRACTED · 26% INFERRED · 0% AMBIGUOUS · INFERRED: 42 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]

## God Nodes (most connected - your core abstractions)
1. `log()` - 14 edges
2. `processAsynchronousMessages()` - 11 edges
3. `sendPatchMessage()` - 10 edges
4. `reloadConfiguration()` - 8 edges
5. `changeServerUrl()` - 8 edges
6. `run()` - 8 edges
7. `checkServerConfiguration()` - 7 edges
8. `parse()` - 7 edges
9. `vacdm()` - 5 edges
10. `DisplayMessage()` - 5 edges

## Surprising Connections (you probably didn't know these)
- `reloadConfiguration()` --calls--> `errorLine()`  [INFERRED]
  /Users/nhath/Documents/Code/vacdm-plugin/src/vACDM.cpp → /Users/nhath/Documents/Code/vacdm-plugin/src/config/ConfigParser.cpp
- `reloadConfiguration()` --calls--> `setUpdateCycleSeconds()`  [INFERRED]
  /Users/nhath/Documents/Code/vacdm-plugin/src/vACDM.cpp → /Users/nhath/Documents/Code/vacdm-plugin/src/core/DataManager.cpp
- `changeServerUrl()` --calls--> `pause()`  [INFERRED]
  /Users/nhath/Documents/Code/vacdm-plugin/src/vACDM.cpp → /Users/nhath/Documents/Code/vacdm-plugin/src/core/DataManager.cpp
- `changeServerUrl()` --calls--> `changeServerAddress()`  [INFERRED]
  /Users/nhath/Documents/Code/vacdm-plugin/src/vACDM.cpp → /Users/nhath/Documents/Code/VCLvACDM/vacdm-plugin/src/core/Server.cpp
- `changeServerUrl()` --calls--> `resume()`  [INFERRED]
  /Users/nhath/Documents/Code/vacdm-plugin/src/vACDM.cpp → /Users/nhath/Documents/Code/vacdm-plugin/src/core/DataManager.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.16
Nodes (14): processAsynchronousMessages(), changeServerAddress(), deletePilot(), postPilot(), resetTobt(), sendDeleteMessage(), sendPatchMessage(), sendPostMessage() (+6 more)

### Community 1 - "Community 1"
Cohesion: 0.18
Nodes (15): checkPilotExists(), consolidateData(), consolidateFlightplanUpdates(), consolidateWithBackend(), deltaEuroscopeToBackend(), handleTagFunction(), pause(), processEuroScopeUpdates() (+7 more)

### Community 2 - "Community 2"
Cohesion: 0.25
Nodes (12): CFlightPlanToPilot(), queueFlightplanUpdate(), changeServerUrl(), checkServerConfiguration(), DisplayMessage(), OnAirportRunwayActivityChanged(), OnFlightPlanControllerAssignedDataUpdate(), OnFlightPlanFlightPlanDataUpdate() (+4 more)

### Community 3 - "Community 3"
Cohesion: 0.25
Nodes (6): errorLine(), parse(), parseColor(), checkWebApi(), getServerConfig(), isReadOnlyAirport()

### Community 4 - "Community 4"
Cohesion: 0.39
Nodes (5): createLogFile(), disableLogging(), enableLogging(), handleLogCommand(), Logger()

### Community 5 - "Community 5"
Cohesion: 0.67
Nodes (0): 

### Community 6 - "Community 6"
Cohesion: 1.0
Nodes (0): 

### Community 7 - "Community 7"
Cohesion: 1.0
Nodes (0): 

### Community 8 - "Community 8"
Cohesion: 1.0
Nodes (0): 

### Community 9 - "Community 9"
Cohesion: 1.0
Nodes (0): 

### Community 10 - "Community 10"
Cohesion: 1.0
Nodes (0): 

### Community 11 - "Community 11"
Cohesion: 1.0
Nodes (0): 

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (0): 

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (0): 

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (0): 

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (0): 

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (0): 

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (0): 

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (0): 

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

### Community 20 - "Community 20"
Cohesion: 1.0
Nodes (0): 

### Community 21 - "Community 21"
Cohesion: 1.0
Nodes (0): 

### Community 22 - "Community 22"
Cohesion: 1.0
Nodes (0): 

### Community 23 - "Community 23"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **Thin community `Community 6`** (2 nodes): `EuroScopePlugIn()`, `EuroScopePlugIn.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 7`** (2 nodes): `vacdm()`, `Pilot.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 8`** (2 nodes): `vacdm()`, `Ecfmp.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 9`** (2 nodes): `vacdm()`, `DataManager.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 10`** (2 nodes): `vacdm()`, `TagItems.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 11`** (2 nodes): `tagitems()`, `TagItemsColor.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 12`** (2 nodes): `vacdm()`, `CompileCommands.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (2 nodes): `vacdm()`, `TagFunctions.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (2 nodes): `vacdm()`, `Server.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `vacdm()`, `PluginConfig.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `vacdm()`, `ConfigParser.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (2 nodes): `main()`, `test_main.cpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (2 nodes): `TEST()`, `test_StringUtils.cpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (2 nodes): `vacdm()`, `Date.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 20`** (2 nodes): `vacdm()`, `Number.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 21`** (2 nodes): `vacdm()`, `String.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 22`** (1 nodes): `main.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 23`** (1 nodes): `Logger.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `log()` connect `Community 1` to `Community 0`, `Community 2`, `Community 3`, `Community 4`?**
  _High betweenness centrality (0.145) - this node is a cross-community bridge._
- **Why does `processAsynchronousMessages()` connect `Community 0` to `Community 1`?**
  _High betweenness centrality (0.051) - this node is a cross-community bridge._
- **Why does `parse()` connect `Community 3` to `Community 1`, `Community 2`?**
  _High betweenness centrality (0.050) - this node is a cross-community bridge._
- **Are the 13 inferred relationships involving `log()` (e.g. with `vacdm()` and `changeServerUrl()`) actually correct?**
  _`log()` has 13 INFERRED edges - model-reasoned connections that need verification._
- **Are the 9 inferred relationships involving `processAsynchronousMessages()` (e.g. with `updateExot()` and `updateTobt()`) actually correct?**
  _`processAsynchronousMessages()` has 9 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `sendPatchMessage()` (e.g. with `run()` and `log()`) actually correct?**
  _`sendPatchMessage()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `reloadConfiguration()` (e.g. with `parse()` and `errorLine()`) actually correct?**
  _`reloadConfiguration()` has 3 INFERRED edges - model-reasoned connections that need verification._
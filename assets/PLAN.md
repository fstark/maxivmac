# Low Memory Globals — Implementation Plan

Completed 2026-04-18.  Commits a2ac3b0..6c4bb27.

All 5 phases implemented:
1. Added QHdr, SysParmType, ScrapStuff, Pattern, Region, Zone, VCB to types.def
2. Created assets/globals.def with 113 globals across 22 THINK C 3 sections
3. Wrote GlobalRegistry parser (src/lang/global_registry.h/.cpp)
4. Wired GlobalRegistry into startup and debugger, removed hardcoded table
5. Rewrote imgui_lomem_tool to use GlobalRegistry with section-based filtering


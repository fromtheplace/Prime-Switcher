# OBS Prime Switcher

Auto-switches scenes with independent per-scene prime-number timers (like your
Advanced Scene Switcher setup), as a native OBS dock.

## Behaviour
- Each enabled scene is a free-running timer with its own prime period (seconds).
- Timers are drift-free (`nextFire += P`), so the prime relationships hold for hours.
- A timer firing for the scene that is already live does nothing.
- Min-dwell (default 1.5 s): a firing too soon after the last cut is dropped.
- Simultaneous firings: longest-since-live wins, then shortest period.
- **Stop = reset.** Stop (button/hotkey) *or any manual scene change* discards all phases.
  Start re-seeds random phases, so each run has a different pattern.
- Settings are locked while running; edit, then Start.
- Studio Mode: sets preview and triggers the current transition.

## Hotkeys (Settings > Hotkeys)
Prime Switcher: Start/Stop, Start, Stop

## Build
Needs OBS 30+ SDK and Qt6. Easiest route: copy `src/` into a project made from
`obs-plugintemplate` and list the .cpp files in its CMakeLists, or use the
CMakeLists here against an installed/obs-deps libobs.

Core tests only (no OBS/Qt):
    cmake -S . -B build -DBUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build

## Layout
- `src/core/`      PrimeScheduler, ISwitcher, prime helpers (no OBS deps, unit-tested)
- `src/backends/`  ObsSwitcher (add AtemSwitcher here)
- `src/ui/`        PrimeDock (Qt6 dock)
- `src/plugin.cpp` module entry, hotkeys, events, save/load

## ATEM later
Implement `ISwitcher` (sources = input IDs, cutTo = program/auto, isExternalChange =
program input differs from last commanded) and select the backend in the dock.

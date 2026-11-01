# BS Fixes

Base-game fixes for Quest Beat Saber 1.45.3:

- Repair the access-token async timeout so a slow request does not block the
  main thread indefinitely.
- Stop the basic-rumble player from sending `StopHaptics` to idle controllers
  every frame, which silences advanced haptics on that controller until the
  game restarts. Each hand still gets one stop after a burst of pulses.

The haptics fix does not change presets, pulse strength/duration, advanced
support checks, or BetterHaptics' backend selection.

Build with `qpm s build`, then package with `qpm qmod zip --offline --skip_build`.

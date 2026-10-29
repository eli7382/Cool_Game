Porsche body response
====================

The Porsche gameplay branch uses corner-load increments to derive a rigid-body
heave, pitch and roll target. Effective front/rear wheel rates (80/140 kN/m),
rear roll rate (160 kN/m) and sprung fraction (90%) are game tuning estimates,
not measured Porsche suspension or PASM settings. The front roll rate matches
the existing estimated front/rear load-transfer moment split.

Body and wheel responses use exact critically damped held-target steps. Each
wheel target matches its sampled terrain height through the current smoothed
body matrix. Terrain slope is therefore not added a second time to wheel
offsets. The original four fixed ground-sampling locations remain an
approximation: this does not solve unsprung tire/road contact dynamics.

The visual wheel travel remains -50 to +50 mm. Tire meshes, suspension solver
range and the painted fenders are unchanged. Known front tire intersections in
the full-travel steering stress sweep remain unresolved. Pickup motion uses
its original code branch.

Validation on 2026-10-29:

- `tests/porsche_load_motion_test.cpp`: 75 acceleration/lateral/normal-load
  combinations conserve resultant force, pitch moment and roll moment;
  rigid-body deflection plane, braking signs, time-step partition and long
  held-target response checks pass.
- Estimated steady 1g braking target: front +17.3622 mm, rear -9.92126 mm,
  pitch +0.636234 degrees. These numbers describe this tuning, not a real car.
- Native executable compiled and linked as `game-load-motion-test.exe` because
  Windows denied replacing the main game executable. `--qa-porsche` exited 0;
  native report: 143 checks, zero failures. This includes basic gameplay
  acceleration/braking and rig comparisons, not a dedicated handling or swept
  clearance certification. The main `game.exe` has not received this build yet.

Reproduce the standalone checks from the repository root with the portable
compiler, with assertions enabled (do not define NDEBUG):

```
.tools/w64devkit/bin/g++.exe -std=c++17 -O2 tests/porsche_load_motion_test.cpp -o .tools/porsche_load_motion_test.exe
.tools/porsche_load_motion_test.exe
```

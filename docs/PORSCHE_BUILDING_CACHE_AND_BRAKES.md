# Building shadow reuse and low-speed braking

The Porsche keeps each building sun-depth target while its snapped light
frame and visible building geometry are unchanged. The cache key uses exact
float bits and the same group bounds as the depth renderer. Safehouse door
motion, house/model transforms, visible sluice animation and height, target
ownership, resolution and world initialization invalidate the appropriate map.
Car doors and steering do not invalidate building-only depth. Car self-shadow
updates retain their existing independent pose cache.

`BW_PORSCHE_BUILDING_CACHE=0` disables reuse for comparison. Explicit forced
preparation still renders fresh maps. Geometry is immutable between world
initializations except for the tracked house and sluice state; future editable
building geometry must extend this invalidation contract.

The hidden native regression passes 152 checks. Its cache test compares
31,457,280 R32 depth pixels with forced fresh renders, with zero differences
across 24 cases at both resolutions and both Porsche slots. Moving mechanisms
change 283,814 tested depth pixels, so those cases exercise actual geometry
changes. See `porsche-building-cache-qa.json`.

The parked benchmark also exposed a low-speed braking dead zone: below
0.18 m/s, the previous direction threshold removed braking force. Porsche
braking now requests only the stopping force needed for the current step.
Static contact is accepted only when all four tire friction circles and
wheel brake budgets can balance the required force and yaw torque. It uses
the existing game tire/surface parameters. Insufficient grip, insufficient
braking, engine drive demand and invalid inputs reject static capture.
The pickup follows its existing force path.

`porsche-brake-hold-qa.json` covers nine force-budget cases and 2,880 real game
frames on a slope: four headings, each with left/center/right steering. Those
cases have zero measured horizontal drift, wheel rotation and yaw drift;
releasing the brake and applying throttle moves the car 1.627 m. This is a
game contact approximation, not an OEM ABS/PSM model or exhaustive surface
validation.

Run `build-native/ninja/bin/game.exe --qa-porsche` for hidden regression.
Performance comparisons use `--qa-porsche-performance` with
`BW_PORSCHE_PERF_ROUTE=parked` or `hangar`, with the cache off/on. Each run
contains low/high chase/cockpit views with ambient occlusion off/on, 24 warmup
frames and 96 measured frames per case. The four named reports and
`porsche-building-cache-performance-review.json` retain raw update counts,
simulation traces, source hashes and local stage timings. Reproduce the
comparison summary with `python scripts/review_building_cache_performance.py`.

All four runs pass, with identical cache-off/on simulation traces. Parked
cases remove all 96 sampled building-map redraws per case; moving cases need
80 redraws at low resolution and 95 at high resolution, versus 96 without
reuse. Parked building-stage median CPU submission falls from 0.964–1.552 ms
to 0.063–0.066 ms on this machine. The moving workload reuses fewer frames
and has no consistent timing improvement. Whole-frame medians vary strongly,
with several cached cases slower, including the parked low cockpit AO case
(51.34 ms off versus 168.72 ms on). These sequential instrumented runs prove
the omitted rendering work, not an overall FPS improvement. They do not
establish 60 FPS or cross-device performance. Tire/fender contact and final
visual quality remain separate unfinished work.

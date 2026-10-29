Static world caster depth pass
==============================

`DrawStaticWorldSunDepth(camera, projection)` renders the actual hangar,
tower, safehouse and sluice geometry into the caller's depth-enabled target.
It uses a depth-output branch in the existing world shader, with its existing
foliage alpha test and exclusion of water/translucent glass. It saves and
restores the scene camera and projection/modelview matrices and flushes the
render batch before resetting the shader mode.

The caller owns the render target, clear, depth mask, culling and blending
states. The callback does not call the main scene renderer or recursively
prepare Porsche shadows. It does not include terrain, vegetation, debris,
characters or other vehicles. The sluice drawing includes its current gate
and wheel state, so a future cache must include the relevant world state.

The native suite exercises a 512-square R32 target with an overhead hangar
view and a distant empty view. It checks finite normalized depths, nonempty
building coverage, an empty background, and the projected depth of an
interior roof point away from seams/vents against the authored roof height.
It also checks restoration of the main camera matrices/framebuffer. This is
limited depth-generation evidence, not a complete caster or shadow oracle.

Production integration
----------------------

The callback now feeds a separate world-space building map for each visible
Porsche. It now uses a 48 m orthographic span, 0.1–100 m depth range, and a camera
40 m along the shared sun direction. High/low maps are 1024/512 square. The
car shader samples the map on explicit texture unit 7, applies receiver-plane
PCF using the existing self-shadow algorithm, and combines the two visibility
terms with their minimum. The building-map binding is cleared after each car
draw so it does not leak into other material passes or QA probes. Both map
slots are released during shutdown.

Maps currently regenerate on each preparation call. This avoids stale shadows
from the moving safehouse door and sluice parts, but adds rendering work.
Cache policy and frame time have not been validated. Building maps do not
replace the separate car-caster maps used to shadow the ground.

A receiver fixture compares the actual car shader at six exterior points
against independent positive rays through the broad hangar core and roof
boxes, away from narrow details. Three lit and three blocked points match
exactly in the initial run. This finite fixture does not validate every PCF
edge, all building details, vehicle body coverage or whole-scene appearance.
Native appearance review near buildings and performance measurement remain
required before considering this lighting work finished. Terrain, vegetation,
debris and other vehicles are still outside this caster callback's scope.

Native car-only appearance follow-up (historical)
------------------------------------------------

`porsche-building-hangar-off.png` and `porsche-building-hangar-on.png` are
matched native captures of the closed Porsche outside the hangar's west wall.
Only the building-shadow binding is toggled. The pair changes 27842 pixels
by more than three channel codes, with maximum difference 92. The complete
native suite passes 147 checks. Visual inspection finds a subtle darkening
of affected car surfaces, while the backdrop remains fixed. This is one
camera and location; it does not establish all shadow edges or temporal quality.
Environment reflections and diffuse sky illumination remain present in shade;
the building map modulates direct sun only. At this earlier stage the ground
did not receive the building map. Appearance under other buildings, moving-shadow stability,
caster-volume boundaries and performance still need review.

Performance diagnostics
-----------------------

`BW_PORSCHE_BUILDING_SHADOWS=0` disables building-map generation for a process;
unset or any other value leaves it enabled. This is a diagnostic environment
switch, not a gameplay setting. The explicit performance run now records a
`building_shadows` CPU/GPU stage nested within the existing `sun_shadows`
stage. Its scope includes both car slots, target preparation and building
depth submissions. It excludes the extra car-shader lookup cost in color and
mirror passes; whole-frame comparisons include that cost but also run-to-run
noise. These timers are inactive during normal gameplay.

`scripts/review_building_shadow_performance.py` compares the saved off/on
reports, verifies matching benchmark settings and checks every recorded speed,
steering and position sample for equality. Use its stated limits when reading
the results; a single instrumented pair is not a general performance claim.

The first sequential pair on the RTX 3050 Laptop at 1280x720 records identical
driving traces in all eight cases. In the four AO-on cases, median building
stage CPU submission is 3.8208–5.24345 ms and median GPU elapsed time is
0.380416–0.958976 ms. Whole-frame medians are worse with shadows enabled,
by 6.90–41.43 ms in those cases. That larger difference includes timing noise
and other stage variation, so it is not assigned wholly to the new pass.
The direct stage cost is still material: reducing needless building
submissions and map refreshes is required next. These base-route captures
do not replace a near-hangar movement/performance test or uninstrumented
normal-game timing.

Whole-building depth culling
---------------------------

The depth callback now tests conservative building-group bounds against the
actual light view/projection before submitting their component geometry.
Hangar bounds include the service pallet; tower bounds include the mast;
sluice bounds include the wheel and full gate position range. Safehouse
bounds combine the imported shell's mesh bounds, furniture bounds and a
procedural envelope for stairs, lamps and the door's full swing. Imported
geometry uses cached rigid-mesh bounds rather than rescanning its vertices.

Passing `groupCull=false`, or disabling mesh visibility, retains the original
submission path for comparison. Native QA compares R32 buffers with group
culling off/on in centred and cropped views of all six groups, using closed
and open doors and two clock values. All 3145728 compared pixels match
exactly; 1688441 are occupied. The complete suite passes 148 checks. This is
finite render-preservation evidence, not exhaustive proof for all camera and
world-state combinations. No geometry or caster category was removed.

In the retained pre-culling ON run versus a later culled ON run, the four
AO-on base-route cases reduce median building-stage CPU submission from
3.8208–5.24345 ms to 0.126–0.1445 ms. Median stage GPU time becomes
0.02304–0.04608 ms, versus 0.380416–0.958976 ms before. All eight recorded
driving traces match exactly. This supports the optimization for the measured
route, where buildings are usually outside the local shadow volume; it does
not imply the same reduction beside a building or a 97% whole-frame gain.
The runs are non-interleaved and instrumented. See the raw reports and
`porsche-building-group-performance-review.json` for their scope and limits.

Hangar benchmark route
----------------------

`BW_PORSCHE_PERF_ROUTE=hangar` selects an explicit benchmark-only starting
point at X=-53, Z=190 outside the west hangar wall, facing +Z. It resets body
and wheel motion before each case, then uses the same warmup, controls,
ordinary gameplay, cameras and HUD as the base benchmark. The report records
`route: hangar`; ordinary mission placement is unchanged. This short route
does not cover every building or driving scenario.

`BW_PORSCHE_BUILDING_GROUP_CULL=0` retains the original full-group submission
path in production shadow preparation for diagnostics. Unset or any other
value enables culling. It does not change the explicit off/on render oracle
inside native QA. For the hangar comparison, building shadows remain enabled
in both runs; only this group-culling switch changes.

Review the resulting reports with:

```
python scripts/review_building_group_performance.py --before docs/porsche-hangar-group-off-performance.json --after docs/porsche-hangar-group-on-performance.json --output docs/porsche-hangar-group-performance-review.json
```

The first sequential hangar pair passes all eight cases in each run, with
identical speed, steering and position traces. The car moves approximately
8.06 m beside the west wall, from X≈-53/Z≈190.37 at the first sampled frame
to X≈-52.01/Z≈198.00 at the last. The retained high-preset chase capture is
`screenshots/porsche-hangar-performance-high-chase.png` (culled run).

For AO-on cases, median building-stage CPU submission decreases from
3.82275–4.9397 ms to 0.58495–0.61535 ms. Stage GPU medians decrease from
0.331264–0.701952 ms to 0.070656–0.48128 ms. This confirms a useful local
reduction while the hangar remains nearby, rather than only on the open base
route. The sequential instrumented comparison does not establish normal-game
FPS, all building workloads, repeated-run variance or cross-device gains.

Shared car/world building receivers
----------------------------------

The world shader now samples the same building maps on separate texture
units 8/9, alongside the existing car-caster maps on 12/13. Direct-sun
visibility uses the minimum of these maps, avoiding double-darkening overlap.
Bindings are cleared after scene passes and before map generation. Map span
increased from 16 m to 48 m for surrounding ground coverage. The outer 8%
of the projected window fades smoothly to lit visibility. Car receivers stay
well inside it. This is local coverage around visible Porsches, not a complete
scene-wide shadow system.

Six world-shader receiver probes (three lit/three blocked) match independent
hangar rays with zero visibility error. Four additional probes outside,
within and beyond one map boundary match analytic smoothstep visibility to
within 0.000613 in the byte output target. The full hidden suite passes 148
checks. These finite probes do not prove every edge or temporal stability.

Matched scene captures are `screenshots/porsche-building-world-hangar-off.png`
and `-on.png`. Both car and world building-shadow bindings are toggled;
the full-viewport image difference is not an isolated-car measurement.
Inspection confirms ground shade beside the car. Environment illumination
remains present: this map modulates direct sun only. The raw evidence is in
`porsche-building-world-receiver-qa.json`, `porsche-building-world-fade-qa.json`
and `porsche-building-world-appearance-qa.json`.

Earlier performance comparisons in this document used the smaller car-only
map. They are not measurements of the new 48 m world-receiver workload.
Updated performance, wider appearance review and moving-map boundary
stability remain open.

World-grid stabilization
------------------------

Building-map orientation is now derived at the origin from the fixed sun
direction. Its centre is snapped in light-space right/up/depth coordinates
to a grid with pitch `48 / resolution` metres. This keeps the view matrix
identical while the requested centre stays within one grid cell. Right/up
cell changes translate the projected world by whole texels. The depth axis
is also snapped to avoid needless normalized-depth drift for stationary
world geometry. Maximum centre displacement is half a cell per axis.

The depth callback accepts the exact stored view matrix. Both drawing and
group culling use it; rebuilding a look-at matrix from translated eye/target
coordinates could otherwise reintroduce floating-point orientation drift.

Native QA renders actual hangar depth at seven within-cell centres for both
512 and 1024 resolutions: all 7864320 compared depth pixels are identical,
including 2902476 occupied pixels. Thirty-six fixed-point projection checks
match integer texel steps with zero measured pixel error. The complete suite
passes 149 checks. The evidence is in
`porsche-building-shadow-stability-qa.json`.

This verifies finite subcell and grid-step behavior. It does not establish
all moving-game shadow edges, fade-band perceptual quality or the frame cost
of the wider world-receiver pass. Animated building parts still regenerate
on each preparation call, and environment illumination remains separate.

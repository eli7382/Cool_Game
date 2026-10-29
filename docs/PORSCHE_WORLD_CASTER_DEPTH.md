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
Porsche. It uses a 16 m orthographic span, 0.1–100 m depth range, and a camera
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

Native appearance follow-up
--------------------------

`porsche-building-hangar-off.png` and `porsche-building-hangar-on.png` are
matched native captures of the closed Porsche outside the hangar's west wall.
Only the building-shadow binding is toggled. The pair changes 27842 pixels
by more than three channel codes, with maximum difference 92. The complete
native suite passes 147 checks. Visual inspection finds a subtle darkening
of affected car surfaces, while the backdrop remains fixed. This is one
camera and location; it does not establish all shadow edges or temporal quality.
Environment reflections and diffuse sky illumination remain present in shade;
the building map modulates direct sun only. The ground does not yet receive
this building map. Appearance under other buildings, moving-shadow stability,
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

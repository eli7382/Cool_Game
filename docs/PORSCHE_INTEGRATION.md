# Porsche integration in progress

The white/red 992 GT3 RS now loads and drives in this native C++20/raylib 5.5
game, alongside the existing pickup. Start a mission and walk to the white/red
car beside the pickup at the base. Press E nearby to enter; E exits at low speed.
Vehicle selection keeps each parked car's identity and position.

## Controls and current behavior

| Control | Action |
| --- | --- |
| W / S | Accelerate, brake, reverse |
| A / D | Steer left / right |
| Space | Brake |
| C | Driver / chase camera |
| O / K | Left / right door, when stopped |
| I | Raise / lower both windows |
| G | Toggle the visual DRS flap |
| L | Toggle headlamp emitters |
| [ / ] | Animate the shift paddles |
| J | Start / pause the Sport Chrono stopwatch |
| U | Stop and zero the stopwatch |
| Home / End | Adjust rear-view mirror yaw |
| Page Up / Page Down | Adjust rear-view mirror pitch |
| Backspace | Reset rear-view mirror aim |
| R | Reset the occupied car at the base |

Wheels spin at their separate front/rear tyre radii; calipers retain their
non-spinning parents. A double-precision joint solver poses the front wishbones
and rear five-link assemblies at each wheel's requested height and steering
angle. Steering rods and antiroll links retain their lengths; dampers telescope,
rear shafts use the source's small plunge approximation, and four springs
compress/extend through their exported morph targets. The physics contact
positions now match the stock wheel centres, and each corner drives its own
named wheel. The visual travel envelope is +/- 50 mm.

Doors, windows, pedals, paddles, tachometer needle and wing surfaces use the
named rig. Braking at speed raises the visual airbrake
and takes priority over DRS. Paddles currently animate without a manual
gearbox. The Porsche currently inherits the game's existing driving model;
Porsche-specific mass, gearing and tire-force handling are unfinished.

The driver camera follows the authored seat anchor and body transform. A
dedicated native shader uses the original artwork UVs, detail UVs, packed
roughness/metalness, normal maps, independent clearcoat normals, emission and
HDR reflections. Glazing uses a thin alpha approximation. Native indirect
lighting now uses a cosine-convolved irradiance map, an eleven-layer
GGX radiance atlas and a correlated-Smith DFG lookup. Clearcoat has its own
normal, roughness and integrated specular response. The split-sum equations
follow [Filament's lighting model](https://google.github.io/filament/Filament.html).
The atlas preserves 1024x512 sharp reflections and at least 128x64 directional
resolution at rough levels, with wrapped horizontal/clamped polar gutters.
HDR samples are not clipped before filtering.
The HDR panorama is converted to linear RGB32F because default raylib 5.5
builds disable HDR image decoding. No library build flags need changing.

The fine front grille switches between physical wires and the existing
prefiltered radiator maps according to projected pitch, with hysteresis.
This keeps unresolved wires out of normal-distance views while retaining
physical detail for close inspection.

The four native screen textures now refresh from game telemetry at up to
10 Hz. They show speed, direction selection (D/R/N), RPM, corner tyre
temperatures and wear, and longitudinal/lateral/normal loads. The selection
indicates the current driving direction; a numbered PDK gearbox is unfinished.
Unmodeled oil, coolant and fuel values are not presented as measurements.
The screen layout is an authored telemetry approximation, not factory firmware.

The Sport Chrono hands and LCD share vehicle-owned elapsed simulation time.
J starts/pauses, U resets, and leaving/swapping vehicles retains each timer.
It stops at 99:59.9. The display framebuffer updates run before the 3D pass,
use flipped V coordinates for render-target sampling, share textures across
detail levels and keep separate targets for occupied and parked cars.
Unchanged telemetry reuses the existing targets. Barlow fonts and their
redistribution license are bundled in `assets/porsche/fonts/`.

All three optical panes now show the native game scene. The reflected camera
and projection follow each fitted pane, including door movement and rear-view
adjustment. Projection is cropped to the actual optical vertices, and an
oblique near plane rejects geometry behind the silvered surface. The plane
construction follows the production Three viewer; the clipping method is
described by [Eric Lengyel](https://terathon.com/lengyel/Lengyel-SelectedPapers.pdf).
These are planar approximations to the source optics, with inferred adjustment
limits of +/- 0.18 rad yaw and +/- 0.12 rad pitch.

Normal rendering updates at most one eligible mirror per frame, with a
30 Hz per-pane limit on High and 15 Hz on Low. Targets are 512x256 or 256x128,
with trilinear mip filtering and up to 8x anisotropy. Panes outside the main view, smaller than ten
pixels or facing away are skipped, as are updates when the viewer is more
than 12 metres from the vehicle.
Image content updates on that budget while projection follows the current
pane and eye. Reflective panes are hidden during mirror passes to prevent
feedback. The existing HDR material remains the fallback. Reflecting scene
content is already display encoded; the final mirror copy applies a linear
silver tint without a second exposure/tone-map pass.

Display and mirror updates run outside 3D mode and restore the main camera,
projection/modelview matrices and viewport. `DrawWorldScene` supplies the same
native world drawing for the main camera and reflected cameras. The render
targets are owned separately from shared model materials and are released at
shutdown. The rear-view adjustment API is `SetPorscheRearview(vehicle,yaw,pitch)`.

The Porsche now casts directional sun shadows onto its own parts. Each vehicle
owns a car-relative R32F depth target: 2048 square on High, 1024 on Low. The
depth pass uses the posed doors, wheels, aero, suspension and spring morphs;
transparent glazing, fine grille wires and coplanar artwork do not become
opaque blockers. Alpha-masked casters retain their alpha test.

A 3-by-3 comparison filter uses the receiver plane's depth at each sampled
texel to prevent bands on sloped panels. It attenuates direct sun lighting;
environment lighting and emission keep their existing contributions. The map
updates before the mirror captures, so scene reflections include these shadows.
Camera motion and world translation reuse an unchanged local map. Body
orientation, pose, caster detail and quality-preset changes invalidate it.
The shadow sampler uses texture unit 11 explicitly; assigning it through
`SHADER_LOC_MAP_ALBEDO + 11` would overlap raylib's bone-attribute locations.

See `docs/porsche-shadow-review.json` for matched exterior, cockpit and open-door
views. Native GPU checks compare 48 ray/box visibility samples, alpha masks,
320 unoccluded pixels on five sloped planes, and 157440 conservative caster-box
corners across 72 posed-car cases. Cache, target ownership, quality changes and
main rendering-state restoration are checked separately. These are finite
correctness checks; the full-game rendering cost still needs profiling.

The same car maps now shade world receivers through the existing world shader.
Roads, terrain, runway boxes and nearby solid geometry receive the car's
directional shadow at their real fragment positions. World texture units 12
and 13 hold the two independently owned maps; the slot outside an active
Porsche remains disabled. Overlapping filtered maps use minimum visibility,
so identical shadows are not applied twice. The Porsche's oval placeholder
is skipped while a directional map is available; other vehicles retain their
existing shadows. The map bindings are cleared after each world/mirror pass.

World receivers behind the map's far plane can still be shadowed: occupied
texels extrude along the sun ray, while clear texels remain lit. This lets a
raised car cast onto the ground without moving its shadow up with the body.
Only direct sun terms are attenuated; environment, emission and local lamps
retain their existing contributions. World geometry casting shadows onto the
Porsche itself remains separate unfinished work.

`docs/porsche-world-shadow-review.json` binds the receiver tests and matched
closed/open/raised-car captures to the executable. The fixtures cover 114
analytic receiver samples in both slots (including points beyond the caster
depth range), 320 unoccluded slope pixels, and partial-filter overlap. A
separate reference traces posed car triangles at nine light-texel centres for
each of 126 actual road/terrain mesh points, including softened edges. It
compares geometry rather than reading the rendered depth values. Masked-only
oracle hits are classified separately; the alpha mask path has a GPU fixture.

World-to-car occlusion, local ambient occlusion, material refinements,
comprehensive clearance, optical appearance and full-game profiling remain in development.
The linkage datums are fitted to the source model; the joint solver supplies
visual articulation and does not replace the game's tire-force simulation.
The requested final visual fidelity is not verified.

## Prepared assets

`assets/porsche/` contains Hero, Near and Far GLBs, shared external textures,
rig metadata and spring morph deltas. Distribute the whole directory. Geometry
is split at 60,000 vertices per primitive for raylib's unsigned-short indices.
Original triangle corners, attributes, morph targets, winding, scene graph,
materials and texture bytes are retained. Hero has 682,643 triangles, Near
218,113 and Far 156,234. These are development assets; final performance is
not established.

`src/breakwater/porsche_asset_data.hpp` provides generated parent indices,
local/rest world matrices, spring spans/limits, morph buffer offsets, and
mesh-to-node bindings. Raylib loads meshes in
node order and bakes node transforms into vertices. A runtime rig must apply
`posedWorld * inverse(restWorld)` to each baked mesh. Parents can appear after
children in the generated table; evaluate them recursively or topologically.

The corrected `SteeringWheel_Pivot` exported local +Z axis points toward the
driver. Rotate around that local axis by `frontSteeringRadians * 8` using the
current visual steering ratio. Positive front steering points toward vehicle
+X, the driver's left. The rim top must move toward that same side near centre.
Do not use the previous negative sign or world-space Z axis.

The shaft is fitted to the circular horn hub. The sculpted outer rim has an
eccentric centre, so fitting the pivot to that rim made the hub visibly orbit
despite passing rigid-rotation checks. The hub-frame correction moves the
pivot by 24.7 mm and preserves the resting mesh and artwork. Source tests cover
all three detail levels, full steering lock, reversals and return to centre;
the previous pivot reproduces a 37.1 mm hub orbit. Measurements and native
driver views are recorded in [the hub review](porsche-steering-hub-review.json).

Raylib's stock loader does not implement all retained glTF physical material
extensions or spring morph controls. The custom renderer interprets the
supported material fields, owns shared texture resources, sorts transparent
surfaces, and lazily loads the required detail level. First use of an uncached
level can stall; loading policy and the final rendering budget need profiling.

## Standalone build on this machine

The original build entry points expect the external benchmark checkout. The
additional `build-native.ps1` and `cmake/standalone/` build the same game sources
with raylib 5.5 without that checkout. The original scripts remain available.

This machine has no installed MSVC C++ toolset. A portable official w64devkit
2.10.1 toolchain and the official raylib 5.5 source checkout are located under
the ignored `.tools/` directory. They are local dependencies, not committed
compiler binaries. `build-native.ps1` accepts `-ToolchainRoot` and
`-RaylibSource` to select other installations.

The official toolchain archive used here was:
<https://github.com/skeeto/w64devkit/releases/download/v2.10.1/w64devkit-x64-2.10.1.7z.exe>
with SHA-256
`add1edf984ce4d473cfeb7c52e0ab38db73a855bba6c7f564d5e16648eb2896a`.
Raylib sources are from <https://github.com/raysan5/raylib/tree/5.5>.

From the repository root:

```powershell
./build-native.ps1
./build-native/ninja/bin/game.exe
```

The game compiles and links with the Porsche implementation through this build
path. Existing benchmark/MSVC entry points are retained; they need their
external benchmark dependencies as before.

## Native checks

```powershell
./build-native/ninja/bin/game.exe --qa-porsche
./build-native/ninja/bin/game.exe --qa-capture=car-front --qa-hidden
```

The Porsche test uses a hidden OpenGL window. It enters the parked car through
the game's interaction path, retains the pickup, loads all three detail levels,
checks native steering geometry and reference poses, then drives, turns and
brakes under the actual game physics. It also captures exterior, open-door and
driver steering views in `screenshots/porsche-native-*.png`. It also compares
stopwatch hand poses to the production Three rig, checks start/pause/resume,
vehicle ownership and 10 Hz refresh behavior, and reads the four real GPU
display targets before/after controlled telemetry changes. The live cockpit
capture uses a deterministic telemetry fixture; its 113 km/h reading is not
a measured driving benchmark. Current results
are in `docs/porsche-native-qa.json`. These are finite development checks;
they do not prove complete collision clearance, final performance or OEM
vehicle dynamics.

Ground shadows now sit near the rendered ground and do not write depth. The
previous raised shadow ellipsoid cut through the Porsche's low body. The
existing pickup capture was repeated after that correction.

## Asset regeneration and checks

With Python and numpy installed and the Porsche authoring project beside this
checkout:

```powershell
python scripts/prepare_porsche_assets.py --source ../Porsche_911
python tests/check_porsche_assets.py --source ../Porsche_911
python scripts/prepare_porsche_environment.py
python scripts/prepare_porsche_ibl.py
python tests/check_porsche_ibl.py
node tests/check_porsche_environment.mjs ../Porsche_911
node tests/build_porsche_reference.mjs ../Porsche_911
node tests/build_porsche_mirror_reference.mjs ../Porsche_911
```

The independent check compares every source triangle corner, including all
vertex attributes and spring morphs, and verifies image bytes and metadata.
Its current result is recorded in `docs/porsche-asset-validation.json`.
The HDR check compares all converted RGB channels against the existing viewer
loader. The native pose reference is generated from the exported models and
the production Three adapter; the game itself requires no JavaScript runtime.

The IBL checker uses independent Gauss-Legendre hemisphere quadrature for DFG,
direct solid-angle cosine integration for diffuse light, constant-environment
preservation, the sharp Fresnel limit and the analytic roughness-one white
furnace result. It also emits seventeen native GPU probe references. The native
test reads those probes through the actual production shader and texture
bindings. See `docs/porsche-ibl-validation.json` and
`docs/porsche-native-lighting-qa.json`. Scene mirrors and car self-shadowing
have separate fixtures and matched-view reviews. Full-vehicle appearance and
the requested final fidelity still require further work.

The mirror fixture compares 54 reflected/cropped/oblique projections against
the existing Three implementation, using actual optical vertices from all
three detail levels and movement, nonuniform scale, door, joint and camera
poses. Native GPU checks move real scene markers behind the vehicle, compare
their positions in the targets and main cockpit, and reject a depth-disabled
marker behind the clipping plane. They also check graphics-preset target
replacement, vehicle ownership, the update budget, and main HUD coordinates.
See `docs/porsche-native-mirror-projection-qa.json`,
`docs/porsche-native-mirrors-qa.json` and the clean/marked cockpit captures.
QA can force all three updates; ordinary rendering retains the one-update
budget. These finite tests do not establish final optical fidelity or frame rate.

The nine-coefficient SH candidate was rejected after it exceeded the diffuse
reference tolerance in darker directions. The retained 128x64 irradiance map
uses the exact roughness-one N=V GGX/cosine kernel. Broad GGX layers use direct
512x256 spherical quadrature; the narrow 0.1 layer uses 1,024 filtered importance
samples. The 128x128 DFG table includes both parameter endpoints and uses an
explicit texel-centre coordinate transform. These choices and the rejected
candidate's measured error are recorded in the validation report.

The suspension check covers 65 compression/front-steer/rear-steer combinations
per detail level. It compares 19,110 actual node frames and 24,960 sampled spring
positions/normals to the production Three rig, alongside independent joint,
link-length, wheel-height and damper-overlap checks. See
`docs/porsche-native-suspension-qa.json` and the rest/cross-axle mechanism
captures in `screenshots/porsche-native-suspension-*.png`. Regenerate its binary
reference when the source asset or rig changes:

```powershell
node tests/build_porsche_suspension_reference.mjs ../Porsche_911
```

Keep `assets/porsche/ATTRIBUTION.md` with distributions. The body and original
cockpit are adapted from Black Snow's CC BY 4.0 model; the rig, wheel assemblies,
finish maps and other additions are described in that attribution.

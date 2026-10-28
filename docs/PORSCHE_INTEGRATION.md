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

Scene-reflecting mirrors, material refinements,
comprehensive clearance and full-game profiling remain in development.
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
`docs/porsche-native-lighting-qa.json`. These checks do not validate local scene
reflections, self-shadowing or full-vehicle appearance; those require separate
work and visual review.

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

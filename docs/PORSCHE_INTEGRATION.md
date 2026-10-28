# Porsche integration in progress

The white/red 992 GT3 RS assets are prepared for this native C++20/raylib 5.5
game. Gameplay and renderer integration are still being implemented. The game
currently uses its existing SUV; adding the asset files alone does not select
the Porsche in gameplay.

## Prepared assets

`assets/porsche/` contains Hero, Near and Far GLBs, shared external textures,
rig metadata and spring morph deltas. Distribute the whole directory. Geometry
is split at 60,000 vertices per primitive for raylib's unsigned-short indices.
Original triangle corners, attributes, morph targets, winding, scene graph,
materials and texture bytes are retained. Hero has 682,643 triangles, Near
218,113 and Far 156,234. These are development assets; final performance is
not established.

`src/breakwater/porsche_asset_data.hpp` provides generated parent indices,
local/rest world matrices and mesh-to-node bindings. Raylib loads meshes in
node order and bakes node transforms into vertices. A runtime rig must apply
`posedWorld * inverse(restWorld)` to each baked mesh. Parents can appear after
children in the generated table; evaluate them recursively or topologically.

The corrected `SteeringWheel_Pivot` exported local +Z axis points toward the
driver. Rotate around that local axis by `frontSteeringRadians * 8` using the
current visual steering ratio. Positive front steering points toward vehicle
+X, the driver's left. The rim top must move toward that same side near centre.
Do not use the previous negative sign or world-space Z axis.

Raylib's stock loader does not implement all retained glTF physical material
extensions or spring morph controls. Native shading, rig animation, driver
camera, vehicle tuning, clearance and in-game profiling still need work.

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

The baseline game has compiled and linked successfully with this build path.
This is build evidence, not a test of a playable Porsche integration.

## Asset regeneration and checks

With Python and numpy installed and the Porsche authoring project beside this
checkout:

```powershell
python scripts/prepare_porsche_assets.py --source ../Porsche_911
python tests/check_porsche_assets.py --source ../Porsche_911
```

The independent check compares every source triangle corner, including all
vertex attributes and spring morphs, and verifies image bytes and metadata.
Its current result is recorded in `docs/porsche-asset-validation.json`.

Keep `assets/porsche/ATTRIBUTION.md` with distributions. The body and original
cockpit are adapted from Black Snow's CC BY 4.0 model; the rig, wheel assemblies,
finish maps and other additions are described in that attribution.

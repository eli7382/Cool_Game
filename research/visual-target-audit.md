# Reference-fidelity audit

Date checked: 2026-10-23. This note records target standards from first-party / developer documentation and compares them with the current Breakwater source. It is an engineering gap assessment, not a claim that a small raylib project can reproduce either studio's full asset pipeline in one pass.

## War Thunder aircraft and cockpit production

Gaijin's aircraft modeling guidance calls for real-scale, separately modeled airframe components, several detail levels (LOD0–LOD3), damage/collision geometry, mapped surfaces, baked ambient occlusion, diffuse/normal/specular maps, aging and service labels. It explicitly mentions landing gear and other moving parts. Its cockpit guidance calls for full geometry suitable for a freely moving 360-degree cockpit camera, animated controls and gauges, and roughly 70–150k triangles for a single-seat cockpit or 100–200k for a two-seat cockpit. These figures are specific to that production pipeline and are not a required target for this renderer.

The current plane and helicopter instead use hand-built primitive geometry in `src/breakwater/world.cpp`; they have animated propeller/rotors and rudimentary cockpit pieces but no authored UV sets, normal/specular maps, damage-state mesh sets, LOD assets, rigged pilots, or full cockpit instrument simulation. Current code is far below the cited aircraft/cockpit standard.

## Forza Horizon 6 vehicle authenticity

In the official FH6 Developer_Direct interview, Playground's design director says the team consulted with Toyota so the 2025 GR GT Prototype and Land Cruiser details would represent both the real vehicles and their in-game versions. The game uses a large roster and a high-production visual pipeline; the cited public interview does not publish a polygon budget or a technical per-car modeling spec. The fidelity target therefore means inspectable, reference-accurate body proportions and cabin, physically grounded materials, authored details, lighting/reflections, and animation/handling that agree with the depicted car. It does not justify inventing a FH6 triangle count.

The current pickup is generic source-built geometry. It has modeled tires, cabin pieces, dashboard elements, lamps and trim, but its boxy body and cockpit remain visibly stylized. It has no scanned/reference-derived mesh, texture atlas, normal/roughness maps, PBR metal/glass shading or detailed deformable body. It is not close to FH6 quality.

## Work order implied by the gap

1. Replace generic silhouettes with correctly proportioned, reference-defined primary forms and individual mechanical components.
2. Add authored texture coordinates and material maps for paint, rubber, glass, metal, fabric, skin and weathering; keep lighting/material response consistent.
3. Add internal cockpit geometry, readable instruments, articulated controls, landing gear/control surfaces and damage states.
4. Improve character anatomy, clothing layers, equipment, face and animation; then apply the same material/lighting standards to buildings and terrain.
5. Build a high-quality art pipeline and make performance profiles handle LOD and culling. The current host has integrated graphics and the local source tree does not contain a studio model-authoring toolchain or licensed vehicle models.

## Sources

- [War Thunder CDK: 3D models for cockpits](https://wiki.warthunder.com/cdk/234-3d-models-for-cockpits) — cockpit polygon range, full camera view, animated gauges/controls, texture sets.
- [War Thunder CDK: 3D models for aircraft](https://wiki.warthunder.com/cdk/232-3d-models-for-aircraft) — research/reference, LOD, damage collision, moving parts and texture workflow.
- [Xbox Wire: Forza Horizon 6 Developer_Direct interview](https://news.xbox.com/en-us/2026/01/22/forza-horizon-6-developer-direct-breakdown-interview/) — first-party description of the vehicle roster, districts and real-vehicle detail validation.

## Follow-up vehicle mesh pass — 2026-10-23

The pickup now has a smoothed body and separate roof loft with UVs, plus a pressed tailgate with ribs, hinges, plate, and reflectors. The helicopter has a smoothed cabin and tapered tail boom, and the plane's existing custom fuselage has a dedicated skin map. Three vehicle-specific generated material maps blend into those shells, and the shader preserves plain-color trim. Fresh High-preset live captures cover exterior and cockpit views for each vehicle (`screenshots/Breakwater_VehicleShell_20261023_212653_*.png`). This is a genuine source-level and visible rendering improvement over the previous ellipsoid shells, but it still does not meet the reference target: the forms remain procedural and stylized, there are no scan/reference-derived assets or authored normal/roughness maps, and the rest of the world is visibly low-poly. The exterior capture tests exited successfully; this visual-only pass did not retest motion or flight behavior.


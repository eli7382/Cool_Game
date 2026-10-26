# F-16A Netz reference and modeling plan

Status: pre-production research and three visual mockups are ready. **No F-16 3D mesh has been started. The mesh begins only after the user approves a mockup and chooses the Netz configuration.**

## Approval target

- The primary shape proposal is `f16-netz-mockups/F16_Netz_orthographic-review.png`. The stormfield image is a lighting/material presentation proposal; the detail sheet is for canopy, landing gear, intake, panel and surface treatment. All three are generated concepts, not engineering drawings.
- Approve the standard Israeli F-16A Block 10 Netz or the War Thunder premium “Netz (Mod)”. Gaijin says its premium presentation differs from the researchable Netz by stabilator design, added countermeasures and an expanded weapon fit including the GBU-31 JDAM. The mockup should be treated as a direction proposal until this choice is made.
- No War Thunder geometry, textures, screenshots or other game assets will be copied. Use public aircraft references and the generated images only as visual direction.

## Shape and scale evidence

- War Thunder’s community-maintained Netz (Mod) page gives 15.1 m length and 9.4 m wingspan. This is useful to match the user’s requested in-game reference, but it is not primary airframe documentation.
- NAVAIR’s F-16A reference gives 14.52 m length, 9.45 m wingspan and 5.01 m height. This is a general F-16A reference rather than a Netz Block 10 drawing. The length figures differ, so 15.1 m remains a provisional game-target scale and will not be treated as a validated Block 10 measurement.
- Before scale is locked, compare orthographic photographs and variant-specific documentation, record the measurement convention (including any nose probe or stores), and mark unknown dimensions. The generated images can invent or distort small forms and must never serve as the sole source for proportions.

## High-detail asset workflow

1. Lock the exact Block 10 configuration and livery after approval. Build a reference board with top, front, side and detail views plus a measurement ledger with confidence notes.
2. Model a metric-scale blockout around fuselage stations, intake, wing planform, leading-edge extensions, vertical fin, stabilators, canopy, gear and nozzle. Compare orthographic silhouettes before adding small detail.
3. Keep canopy, landing gear and doors, wheels, control surfaces, nozzle, pylons and stores as independent, correctly pivoted parts. Use real geometry for silhouette-critical forms; model crisp but restrained panel gaps, vents, fasteners, lights, probes and access panels.
4. Create a dense source mesh for curvature and bevel detail, then a real-time mesh with clean normals, smoothing groups and intentional bevels. Avoid deep panel grooves that break highlight flow.
5. Unwrap after major geometry is stable. Keep texel density consistent, place seams deliberately, preserve padding, and inspect a distortion grid. Bake tangent-space normal and ambient-occlusion maps from the detailed source; author distinct painted metal, bare metal, rubber, glass, exhaust and fabric response rather than one generic finish.
6. Export glTF/GLB with applied scale, consistent axes, correct normals, compact material sets and useful pivots. Validate the exported asset in the actual raylib renderer, not only in the authoring application.
7. Build controlled LODs and a low-cost path for integrated graphics. Inspect silhouette, cockpit readability, livery, materials, animation, gear, collision bounds, memory and frame-time at Low and High presets.

## Flight and contact model plan

Use SI units and the chosen airframe’s documented mass, wing area, center of gravity, engine configuration, inertia estimates and control limits. Compute lift and drag from dynamic pressure and air-relative speed, with air density and wind from the game atmosphere. Implement thrust, lift/drag curves, stall, control authority, banked lift, ground roll, wheel braking, crosswind, landing loads and structural-G effects as explicit terms. Public aerodynamic coefficients, inertia and control-law details may be incomplete; record estimates and calibrate against published performance bounds. The result must not be described as an exact real-world flight model.

## Authoring environment and runtime constraints

- The current game is C++20, raylib 5.5 and OpenGL 3.3. Its existing GLB import/render path and Low/High presets can host a detailed asset, but the F-16 asset, materials, LODs and flight model have not been implemented or locally verified.
- Blender is not discoverable as `blender.exe` on this host. If the approved model needs Blender, install a project-local copy under this run; do not alter the pinned compiler, raylib or shared software.
- Current target hardware includes integrated Intel Iris Xe graphics. Use baked detail, sensible texture budgets, LODs and a lightweight material path; GPU utilization and GPU memory are not presently measurable here.

## Sources consulted

- [War Thunder: F-16A Block 10 Netz announcement](https://warthunder.com/en/news/9416-shop-hornets-sting-pre-orders-ec-665-tigre-had-e-and-f-16a-block-10-netz-en) — first-party game reference; variant differences.
- [War Thunder Wiki: Netz (Mod) variant data](https://wiki.warthunder.com/unit/f_16a_block_10_netz_mod) — community-maintained game stats and dimensions, not primary aircraft authority.
- [NAVAIR: F-16 Fighting Falcon (Viper)](https://www.navair.navy.mil/product/F-16-Fighting-Falcon-Viper) — official generic F-16A configuration and dimensions.
- [War Thunder CDK: 3D models for aircraft](https://wiki.warthunder.com/cdk/232-3d-models-for-aircraft) — reference, component, LOD, damage and texture workflow guidance.
- [War Thunder CDK: 3D models for cockpits](https://wiki.warthunder.com/cdk/234-3d-models-for-cockpits) — cockpit geometry and animation guidance.
- [Blender Manual: render baking](https://docs.blender.org/manual/en/dev/render/cycles/baking.html) — normal and ambient-occlusion bake workflow.
- [Blender Manual: UV operators](https://docs.blender.org/manual/en/4.4/modeling/meshes/editing/uv.html) — UV workflow.
- [Blender Manual: bevel modifier](https://docs.blender.org/manual/de/4.2/modeling/modifiers/generate/bevel.html) — bevel workflow.

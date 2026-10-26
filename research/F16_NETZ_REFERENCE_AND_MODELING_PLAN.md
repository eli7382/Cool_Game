# F-16A Netz reference and modeling plan

Status: the user approved the orthographic mockup as the geometry basis and selected the standard Netz configuration. A Blender source asset and game-ready GLB now exist and are integrated into the game. The current aircraft has substantial modeled surface and cockpit detail, but inspection in the actual game still shows a stylized result below the requested War Thunder reference fidelity; this is an active refinement target, not a completed quality gate.

## Approved visual and configuration basis

- The approved geometry basis is `f16-netz-mockups/F16_Netz_orthographic-review.png`. The stormfield image guides lighting and materials; the detail sheet guides the canopy, landing gear, intake, panels and surface treatment. These generated images are visual direction, not engineering drawings.
- The selected configuration is the standard Israeli F-16A Block 10 Netz, not the War Thunder premium “Netz (Mod)”. The model includes a representative M61 and AIM-9 loadout. Its model and gameplay systems are not asserted to reproduce classified or proprietary aircraft/weapon systems.
- No War Thunder geometry, textures, screenshots or other game assets will be copied. Use public aircraft references and the generated images only as visual direction.

## Shape and scale evidence

- War Thunder’s community-maintained Netz (Mod) page gives 15.1 m length and 9.4 m wingspan. This is useful to match the user’s requested in-game reference, but it is not primary airframe documentation.
- NAVAIR’s F-16A reference gives 14.52 m length, 9.45 m wingspan and 5.01 m height. This is a general F-16A reference rather than a Netz Block 10 drawing. The length figures differ, so 15.1 m remains a provisional game-target scale and will not be treated as a validated Block 10 measurement.
- Before scale is locked, compare orthographic photographs and variant-specific documentation, record the measurement convention (including any nose probe or stores), and mark unknown dimensions. The generated images can invent or distort small forms and must never serve as the sole source for proportions.

## High-detail asset workflow and current status

1. **Completed as a prototype:** lock the standard Netz basis, generated reference board and metric-scale model workflow. Variant dimensions still need stronger primary references.
2. **Implemented, still under visual review:** model fuselage, intake, wing planform, leading-edge extensions, fin, stabilators, canopy, cockpit, gear, nozzle, pylons, tanks, stores, panels and surface details as named Blender components.
3. **Implemented in the source model:** separate animated control surfaces, landing gear and doors, wheels, pylons and AIM-9 stores. The latest pass replaced thin tire rings with rounded, closed sidewall/shoulder profiles and a bead detail.
4. **Implemented:** a 111-object Blender source model with 161 raylib mesh primitives and 30 materials, plus generated normal/roughness textures. These counts describe asset complexity, not realism.
5. **Partially validated:** export the asset to GLB and generate Blender renders. The game renderer captures the actual in-game plane from gear, close and top views; Blender still gives a cleaner material result than the game renderer.
6. **Not met:** high-fidelity silhouette, canopy/cockpit realism and game lighting remain below the approved mockup/War Thunder target. No calibrated variant-specific engineering drawing or professional external model has been obtained.
7. **Open:** inspect LOD transitions, collision bounds, model memory and controlled frame-time on Low and High presets after the next visual pass.

## Flight and contact model plan

Use SI units and the chosen airframe’s documented mass, wing area, center of gravity, engine configuration, inertia estimates and control limits. Compute lift and drag from dynamic pressure and air-relative speed, with air density and wind from the game atmosphere. Implement thrust, lift/drag curves, stall, control authority, banked lift, ground roll, wheel braking, crosswind, landing loads and structural-G effects as explicit terms. Public aerodynamic coefficients, inertia and control-law details may be incomplete; record estimates and calibrate against published performance bounds. The result must not be described as an exact real-world flight model.

## Authoring environment and runtime constraints

- The current game is C++20, raylib 5.5 and OpenGL 3.3. The GLB is loaded by the game. Current Low/High lighting and material handling constrain its appearance; the actual game image is the authority for runtime presentation.
- Blender 5.2.3 is installed project-locally under `tools/`; the pinned compiler, raylib and shared software were left unchanged.
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

## 2026-10-27 tire and current-build QA refresh

- Rebuilt the F-16 tires with a closed, lathed section, rounded shoulder and sidewall, and a bead transition. The Blender side render shows a solid rounded tire; a fresh gear-down game render confirms the updated mesh is present in the runtime asset.
- Rebuilt the pinned Windows executable after exporting the revised GLB. The seeded landing fixture passed at 73.10 m/s with landing gear down, inside runway bounds, and a measured −2.57 m/s touchdown sink rate. This remains a short seeded final-approach fixture, not a normal takeoff-and-return route.
- Current silent scripted runs passed the seeded F-16/tree breakup, hostile F-16, M61 and AIM-9 cases. The hostile F-16 remained airborne; the breakup fixture did not fell the tree. These are deterministic gameplay fixtures and do not validate real-aircraft or weapon performance.
- Captures reviewed in the actual renderer: `screenshots/Breakwater_LiveQA_hostile-fixed-wing-gear_1793053242111.png`, `screenshots/Breakwater_LiveQA_hostile-fixed-wing-close_1793053349004.png`, and `screenshots/Breakwater_LiveQA_hostile-fixed-wing-top_1793053358297.png`. They expose the current stylized cockpit/surface rendering and are not evidence of the requested photoreal target.

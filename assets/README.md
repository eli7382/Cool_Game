# Breakwater assets

The world and vehicle paint images were created for this run with OpenAI ImageGen. The normal and roughness maps are derived from the paint images by project scripts. The vehicle maps bind to UV-mapped surfaces; High also loads the credited CC0 vehicle models:

| File | Use |
|---|---|
| `coastal_asphalt.png` | Repeating asphalt albedo for the service road and runway surfaces. |
| `salt_concrete.png` | Concrete albedo for sluice structures, pads, and hardscape. |
| `marsh_ground.png` | Repeating marsh/soil albedo for the catchment terrain. |
| `storm_sky.png` | Equirectangular storm-cloud and sea image mapped to the sky sphere. |
| `storm_sky_v2.png` | Retained storm-cloud panorama iteration. |
| `storm_sky_v3.png` | Retained storm panorama iteration; the v4 derivative has pixel-matched horizontal edges. |
| `storm_sky_v4.png` | Active storm panorama used by the sky sphere and vehicle/water reflections; the edge-processing script reports zero RGB edge mismatch after wrapping. |
| `marsh_ground_v8.png` | Active generated damp soil and forest-litter tile; used at a nine-meter terrain interval with low-frequency shader mottle. |
| `tree_bark_coastal_v2.png` | Active generated bark color tile for procedural trunks and branches. |
| `tree_needles_coastal_v3.png` | Active generated pine-needle color tile for the seven procedural pine variants. |
| `tree_leaves_coastal_v3.png` | Active generated broadleaf color tile for the seven procedural broadleaf variants. |
| `tree_pine_distant_v1.png` | Generated cutout color image used by the distant pine billboard LOD. |
| `tree_broadleaf_distant_v1.png` | Generated cutout color image used by the distant broadleaf billboard LOD. |
| `vehicle_paint_car_v11.png` | Active generated sand-olive rescue-enamel base color on the imported SUV body shell and fallback pickup. |
| `vehicle_paint_plane_v11.png` | Active generated ivory coating tile on broad High-preset airframe shells; small livery and fittings retain their authored materials. |
| `vehicle_paint_helicopter_v11.png` | Active generated gray-green rescue-enamel tile on broad High-preset cabin shells; small livery and fittings retain their authored materials. |
| `vehicle_paint_car_v10.png` | Retained prior SUV enamel base-color iteration; replaced by v11. |
| `vehicle_interior_car_v1.png` | Generated repeating dark automotive soft-touch/vinyl color tile mapped to SUV door cards, seats, and instrument fascia. |
| `vehicle_interior_aircraft_v1.png` | Generated repeating charcoal woven cabin-vinyl color tile mapped to fixed-wing and helicopter seats and cockpit trim. |
| `vehicle_paint_car_v8.png` | Retained prior SUV graphite-enamel iteration; replaced by v9 on the imported SUV and fallback pickup. |
| `vehicle_paint_plane_v9.png` | Retained prior aircraft-coating iteration; replaced by v11. |
| `vehicle_paint_helicopter_v9.png` | Retained prior helicopter-coating iteration; replaced by v11. |
| `vehicle_paint_car_v9.png` | Retained prior storm-slate SUV enamel iteration; replaced by v10 on the imported body shell and fallback pickup. |
| `vehicle_paint_plane_v8.png` | Retained previous aircraft coating tile; replaced by v9 on the broad shells and fallback surfaces. |
| `vehicle_paint_helicopter_v8.png` | Retained previous helicopter coating tile; replaced by v9 on the broad shells and fallback surfaces. |
| `vehicle_paint_car_v7.png` | Retained prior SUV paint-map iteration; no longer sampled by the current High-preset SUV. |
| `vehicle_paint_car_v6.png` | Retained paint-map iteration; no longer sampled by the current high-preset SUV. |
| `vehicle_paint_car_v5.png` | Retained paint-map iteration; no longer sampled by the current high-preset SUV. |
| `vehicle_paint_plane_v5.png` | Retained earlier aircraft coating detail; v8 is now active. |
| `vehicle_paint_helicopter_v5.png` | Retained earlier helicopter coating detail; v8 is now active. |
| `vehicle_normal_car_v11.png` | Subdued tangent-space normal detail derived from the matching v11 SUV paint map. |
| `vehicle_normal_plane_v11.png` | Tangent-space normal detail derived from the matching v11 aircraft coating map. |
| `vehicle_normal_helicopter_v11.png` | Tangent-space normal detail derived from the matching v11 helicopter coating map. |
| `vehicle_normal_car_v10.png` | Retained prior SUV normal-map iteration. |
| `vehicle_normal_car_v8.png` | Retained prior SUV normal-map iteration. |
| `vehicle_normal_plane_v9.png` | Retained prior aircraft normal-map iteration. |
| `vehicle_normal_helicopter_v9.png` | Retained prior helicopter normal-map iteration. |
| `vehicle_normal_car_v7.png` | Retained prior SUV normal-map iteration. |
| `vehicle_normal_car_v6.png` | Retained prior normal-map iteration. |
| `vehicle_normal_car_v5.png` | Retained prior normal-map iteration. |
| `vehicle_normal_plane_v5.png` | Retained prior plane normal-map iteration. |
| `vehicle_normal_helicopter_v5.png` | Retained prior helicopter normal-map iteration. |
| `vehicle_roughness_car_v11.png` | Procedural smooth-enamel roughness variation derived from the matching v11 SUV paint map. |
| `vehicle_roughness_plane_v11.png` | Procedural roughness variation derived from the matching v11 aircraft coating map. |
| `vehicle_roughness_helicopter_v11.png` | Procedural roughness variation derived from the matching v11 helicopter coating map. |
| `vehicle_roughness_car_v10.png` | Retained prior SUV roughness-map iteration. |
| `vehicle_roughness_car_v8.png` | Retained prior SUV roughness-map iteration. |
| `vehicle_roughness_plane_v9.png` | Procedural roughness variation derived from the matching v9 aircraft coating map. |
| `vehicle_roughness_helicopter_v9.png` | Procedural roughness variation derived from the matching v9 helicopter coating map. |
| `vehicle_roughness_car_v7.png` | Retained prior SUV roughness-map iteration. |
| `vehicle_roughness_car_v6.png` | Retained prior roughness-map iteration. |
| `vehicle_roughness_car_v5.png` | Retained prior roughness-map iteration. |
| `vehicle_roughness_plane_v5.png` | Retained prior plane roughness-map iteration. |
| `vehicle_roughness_helicopter_v5.png` | Retained prior helicopter roughness-map iteration. |

Earlier unsuffixed and prior versioned textures are retained as iteration history. The current SUV, airplane, and helicopter broad shells use v13 color tiles and matching v13 normal/roughness maps; authored source albedos remain on smaller detailed/livery materials. The two cabin tiles provide color/albedo only and have no independent normal or roughness maps. `scripts/make_vehicle_textures_seamless.ps1` crossfades outer edges of selected generated color tiles. Normal images are derived from color-image luminance by `scripts/generate_vehicle_normals.ps1 -Version current`; roughness images use local luminance gradients and per-vehicle response. These are approximations, not independent sculpt bakes, measured data, or artist-authored PBR maps. Textures use mipmaps and trilinear filtering.

The High graphics preset loads the CC0 glTF exteriors listed in `VEHICLE_ASSET_CREDITS.md`: 322 SUV meshes, 93 single-engine aircraft meshes, and 103 helicopter meshes. The aircraft and helicopter include authored 2048×2048 images; the SUV's source paint image is 64×64. High substitutes generated v13 paint and derived surface maps on broad shells. The Low preset and far-distance fallback use C++ source-built vehicle shells and components. Cockpit instruments, wheel/rotor/propeller motion, steering, and aircraft control surfaces receive game-specific animation. In High, the imported SUV receives 78 shallow tread blocks per tire positioned from measured GLB bounds and animated by the same wheel pivots. The helicopter first-person cockpit uses a separate runtime interior because the imported cabin shell obscures the pilot view; the imported helicopter remains in exterior and chase views. All vehicle renders retain a stylized look and are not photorealistic scanned models. Most effects and all continuous ambience/vehicle audio are created at runtime. Sixteen narrator WAV files are bundled under `audio/narrator/`; they were synthesized with the locally installed Microsoft Zira Desktop en-US voice through Windows System.Speech. Four added clips call out each gauge task and the final beacon task when the player approaches. Redistribution rights for these generated voice recordings were not independently confirmed.

The latest vehicle appearance pass adds draw-time details fitted to the imported model bounds: SUV measured-position tire tread blocks, drilled brake discs, retainers, crossbars, supported lightbar, side steps, recovery eyes, parking sensors and headlamp projector optics; aircraft wing ribs, fasteners, pitot, visible flush rescue bands, layered wheel hubs/brakes and cambered animated control surfaces; helicopter cabin fasteners, a segmented reflective rescue band fitted to the sampled shell curve, rescue hoist/fairlead/cable/hook, tail aerial, rotor mast/head/pitch links, skid braces, door rails and a landing/searchlight. The helicopter's authored two-blade rotor remains visible through normal operating RPM; a neutral graphite albedo keeps the near-black source texture from erasing the blade silhouette. Corrected two-sided GLB shading and the airplane propeller/helicopter rotor blur planes. The source GLBs remain unchanged. Fresh High-preset exterior/front/cockpit captures for all three vehicles are listed in `results.json`; those nine processes validate launch and rendering, not hands-on vehicle operation.

The project has no scanned/photogrammetry assets, rigged character assets, or commercial vehicle models. The three imported vehicle exteriors are CC0 1.0; their source pages and the included-file status are recorded in `VEHICLE_ASSET_CREDITS.md`. The normal and roughness maps remain generated approximations, not independently sculpt-baked or measured material data.

The audio implementation uses raylib streams and generated effects. It provides separate mix levels, narrator ducking across buses, tide-responsive coastal surf, progress/threat-adaptive score layers, RPM/load-linked multi-harmonic vehicle sound, distance falloff, stereo panning, material-dependent impacts, weapon-specific reports, and a layered rocket blast. Terrain and obstacle volumes are sampled along source-listener lines; submerged sources/listeners select a muffled sample and receive additional attenuation. Distant reports receive two delayed echo taps, with a strict queue cap. This is a lightweight approximation, not HRTF, a full acoustic geometry solver, recorded vehicle performances, or convolution reverb.

## Vehicle appearance refinement — 2026-10-24

- Added two generated repeating cabin color tiles for door/seat/dashboard surfaces and mapped them onto the SUV and aircraft/helicopter cockpit geometry. The tiles add albedo texture only; they are not scanned materials or complete PBR sets.
- Generated a v9 storm-slate metallic-enamel SUV base-color tile and locally derived a restrained tangent-normal map and smooth-enamel roughness map. Updated only the SUV paint binding; the plane and helicopter keep their embedded authored 2K albedos and existing v8 detail maps.
- Replaced the SUV's straight-looking bumper pins with seated closed recovery loops. The build also retains the measured tire tread, discs, suspension links, aviation controls, cockpit glazing, and detailed analog/MFD panels.
- The pinned Windows x64 wrapper build succeeded after a sandboxed attempt failed on Visual Studio's SDK registry lookup and the unchanged wrapper was rerun with the needed registry access. The incremental compile displayed no warnings. Eleven High-preset framebuffer captures from the final executable (nine exterior/front/cockpit views and SUV wheel/suspension close-ups) each exited 0; fresh captures are recorded in `results.json`.
- The new SUV paint is visibly a little bluer and less coarse than v8, while the detailed tire and bumper hardware remains attached. Visual inspection still shows flat-shaded/stylized models, glazing that reads pale in some exterior angles, and a low-detail storm-coast environment. The requested Forza Horizon / War Thunder / photoreal target remains unmet. This was render QA only; no fresh vehicle-driving, takeoff/landing, combat, or mission-loop test was run.


A higher-detail scanned rescue-helicopter candidate is recorded as source S535 (`https://www.artec3d.com/3d-models/air-rescue-helicopter`, CC BY 4.0), but was not downloaded or included: its 246 MB OBJ link points to a CDN host that did not resolve from this environment. NASA's RAVEN page (S536) also lists glTF research-aircraft models, but the experimental eVTOL geometry does not match this game's conventional rescue prop plane. The shipped vehicle set remains the three CC0 models listed in the credits file.

## Coastal vegetation and terrain textures

The current world bindings use these project-local OpenAI ImageGen color/albedo assets:

| File | Use |
|---|---|
| `forest_floor_coastal_v1.png` | Current damp leaf, moss, clover and fine-grass color tile blended into forest groves. |
| `marsh_ground_v2.png` | Retained lower-contrast marsh floor tile iteration. |
| `marsh_ground_v3.png` | Retained earlier repeating marsh-floor tile iteration. |
| `marsh_ground_v7.png` | Retained generated wet forest-floor iteration; replaced by v8. |
| `marsh_ground_v6.png` | First generated iteration of the wet forest-floor tile; kept for comparison, replaced by the edge-refined v7 tile. |
| `tree_bark_coastal_v1.png` | Retained bark color tile iteration. |
| `tree_needles_coastal_v2.png` | Retained pine-needle color tile iteration. |
| `tree_leaves_coastal_v2.png` | Retained broadleaf color tile iteration. |

`tree_needles_coastal_v1.png` and `tree_leaves_coastal_v1.png` are earlier generated iterations and remain in the project. The vegetation, marsh, and sky images are albedo/color only; they do not include independent normal, roughness, or measured PBR maps. `marsh_ground.png`, v2/v3/v6/v7 and all prior generated textures remain preserved as iteration history. The v8 ground processor reports zero horizontal and vertical edge mismatch after wrap processing; the terrain samples it over nine-meter intervals and adds subtle broad shader mottle. `storm_sky_v4.png` is a seamless derivative of the storm panorama; the wrap processor reports zero left/right edge mismatch in the saved image. It is not a captured HDR environment, and rendered scene quality remains stylized.

## Final environment and vehicle bindings — 2026-10-25

- The current executable binds the generated v13 vehicle paint tiles and locally derived v13 normal/roughness maps on all three broad vehicle shells. These are visual refinements to existing CC0 meshes, not replacement photogrammetry or measured PBR materials.
- Current environment bindings use `marsh_ground_v8.png`, `forest_floor_coastal_v1.png`, `storm_sky_v4.png`, bark v2, pine needles v3, broadleaf leaves v3 and two distant-tree cutouts. Seven source-built tree variants per species are placed across 17 ground-cover centers; High keeps full 3D tree geometry to 36 m and Low to 24 m before switching to cutouts.
- The local final refinement used five additional ImageGen calls (three vehicle finish tiles and two ground-tile iterations); five calls were added to the cumulative 45, for 50 tool invocations. The results ledger reports 43 generated outputs and 40 individually listed project-local paths; one generated ground iteration was not integrated. Image-generation usage does not imply scanned or photogrammetric assets.

## Later texture and forest refinements — 2026-10-25

- Vehicle paint iterations v12 and v13 were generated for the car, airplane and helicopter. The current broad-shell bindings use v13. Their normal and roughness companions were derived locally from image luminance and are not measured or sculpt-baked material maps.
- `forest_floor_coastal_v1.png` is the current damp leaf-and-moss color tile. It blends over the 17 forest-floor centers with an 82 m radius and avoids roads, mission gates, low terrain and the aircraft corridor.
- The original 15-grove tree sequence remains stable. Two separate foothill groups add 168 High / 56 Low tree placement candidates. High uses 1,080 base tree candidates, 800 shrubs, 1,350 grass tufts and 1,750 ferns; Low uses 360, 280, 450 and 620. Placement candidates are filtered by terrain, clearance and camera view, so visible counts vary by viewpoint.
- The current project record counts 57 ImageGen invocations, 50 generated outputs and 47 individually listed project-local image paths, including failed attempts in the invocation total. Token and cost usage are unavailable. These counts do not imply photogrammetry or scanned PBR materials.

## Safehouse assets

The safehouse uses one CC0 1.0 two-storey house shell and three individual CC0 furniture models (sofa, desk and bed). Source pages, authors where credited, license notes and included file sizes are listed in `WORLD_ASSET_CREDITS.md`. The building and furniture are distinct imported models; stairs, the hinged door, lighting and their gameplay integration are project-authored. The current renderer captures show a functional art blockout, not a photoreal interior.

## F-16A Netz asset

`f16_netz.glb` is an original project-authored Blender model based on the approved orthographic standard-Netz concept. It has 112 exported meshes, 156 raylib primitives, 30 materials and nominal dimensions of 15.15 m length including pitot, 9.45 m span and 4.94 m tail height. `f16_netz_source.blend` is the editable scene; `scripts/build_f16_netz.py` rebuilds the GLB, inspection renders, mesh map and C++ index header with the bundled Blender 5.2.3 installation. The two wingtip AIM-9-class store ranges have generated mesh IDs and are removed individually when launched.

`f16_netz_camo_generated.png` is the active generated tri-color paint albedo. `f16_netz_surface_microdetail_v1.png` is a generated supporting surface tile; the Blender script derives the current `f16_netz_normal.png` and `f16_netz_roughness.png` from that tile. These maps are approximate texture responses, not measured or sculpt-baked PBR maps. `f16_netz_camo_generated_v1.png` and `_v2.png` retain earlier generated iterations. The model is made in-house for this run; no third-party F-16 mesh or War Thunder asset is included. The latest game-runtime weapon pass ties the visible stores to a forward-cone IR lock, line of sight, guided flight, impact/proximity effects and ammo state. The model remains visibly stylized and does not meet the approved mockup's photoreal fidelity.

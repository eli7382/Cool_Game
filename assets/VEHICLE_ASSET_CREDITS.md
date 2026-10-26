# Vehicle model credits

The three high-detail vehicle exteriors and cockpit meshes are original, unbranded models from Innerscene. The individual asset pages identify each model as CC0 1.0 and list the design as original. The GLB files are included unmodified in this project; the game applies runtime material handling and rotates the source wheel, propeller, and helicopter rotor meshes around vehicle-specific pivots.

| Project file | Source listing | License | GLB size |
| --- | --- | --- | ---: |
| vehicle_suv_cc0.glb | [Charcoal five-door crossover SUV](https://www.innerscene.com/tools/library/3d-parts/charcoal-five-door-crossover-suv-a1996830) | CC0 1.0 | 5,081,324 bytes |
| vehicle_plane_cc0.glb | [Single-engine propeller aircraft](https://www.innerscene.com/tools/library/3d-parts/single-engine-propeller-aircraft-d6091ef1) | CC0 1.0 | 8,061,320 bytes |
| vehicle_helicopter_cc0.glb | [Light helicopter](https://www.innerscene.com/tools/library/3d-parts/light-helicopter-eab26245) | CC0 1.0 | 5,317,572 bytes |

The models provide embedded albedo images and authored material factors. The renderer uses those material values for metalness and roughness, plus a game-specific OpenGL 3.3 lighting/fog shader. Transmission is approximated by a tinted alpha pass for glazing; it is not a full path-traced or transmission-capable PBR renderer.

## Generated vehicle material tiles

Three project-local base-color tiles were generated with the built-in OpenAI ImageGen tool and are included under `assets/`. ImageGen prompts, summarized without changing their intended use:

| File | Prompt summary | Use |
| --- | --- | --- |
| `vehicle_paint_car_v10.png` | Seamless, photoreal-natural albedo tile for unbranded coastal rescue 4x4 painted panels; muted sand-olive enamel, fine orange-peel grain, hairline scratches and salt-air haze; neutral diffuse lighting, no logos, text, or geometry. | Broad SUV body shell and source-built fallback. |
| `vehicle_paint_plane_v9.png` | Seamless natural albedo for a small fixed-wing rescue aircraft skin; warm ivory aircraft-grade coating with fine powder-coat grain, faint salt-air haze and small scuffs; no logos, text, or geometry. | Broad airframe shells and fallback. |
| `vehicle_paint_helicopter_v9.png` | Seamless natural albedo for a light rescue-helicopter airframe; muted gray-green satin enamel with subtle powder-coat grain, rubbed edges and salt-air wear; no logos, text, or geometry. | Broad cabin shells and fallback. |

Matching tangent-normal and roughness maps were derived locally from these three color tiles with `scripts/generate_vehicle_normals.ps1 -Version current`. The script estimates normal response from image luminance gradients and roughness from local luminance variation. These are approximate texture conversions, not measured/scanned surfaces, hand-authored PBR maps, or sculpt bakes. The maps modestly improve material response but do not increase source mesh detail. The image generation calls occurred during the vehicle-appearance continuation; cumulative call and asset counts are recorded in the project `results.json`.

## F-16A Netz model

The F-16A Block 10 standard-Netz mesh was authored for this project in Blender 5.2.3 using `scripts/build_f16_netz.py`. The user approved the orthographic mockup in `research/f16-netz-mockups/F16_Netz_orthographic-review.png`. The asset uses project-authored geometry and markings; it does not include a War Thunder model or third-party aircraft mesh. Generic aircraft dimensions and the modeling pipeline were informed by [NAVAIR's F-16 product page](https://www.navair.navy.mil/product/F-16-Fighting-Falcon-Viper), the [USAF F-16 fact sheet](https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104505/f-16-fighting-falcon/) and the [War Thunder CDK cockpit-modeling guide](https://wiki.warthunder.com/cdk/234-3d-models-for-cockpits). Those sources informed research and do not validate the finished model's fidelity.

The active broad tri-color Netz albedo is the generated `f16_netz_camo_generated.png`. The supporting paint microdetail tile `f16_netz_surface_microdetail_v1.png` was generated with OpenAI ImageGen. Current normal and roughness response maps are derived by the Blender builder from that image and are approximations rather than measured or sculpt-baked PBR maps. The GLB, editable blend scene, material assets and C++ mesh-index header are all local project outputs; Blender inspection captures are in `screenshots/F16_Netz_Blender_*.png`.

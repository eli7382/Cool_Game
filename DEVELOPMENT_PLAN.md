# Breakwater: High Water — development plan

## Run boundary and starting condition

- Benchmark work begins 2026-10-23T12:01:40Z; the workspace setup itself is not counted.
- Use the fixed MSVC 19.44 / C++20 / raylib 5.5 / OpenGL 3.3 / x64 Release profile from `toolchain-lock.json`. Keep `build.ps1` and the profile include unchanged.
- The assigned workspace already contains a RIFTWAKE source/build/results set, screenshots, and a separate `Lowwater/` subtree. Treat all of those as pre-existing protected material. Do not inspect or reuse the `Lowwater/` project. Keep RIFTWAKE's `src/main.cpp` unchanged and select only the new source from the build file.
- The machine inventory call was denied. `toolchain-lock.json` identifies a 12-logical-processor Intel CPU; the local setup notes warn that integrated graphics are limited. CMake and the compiler are pinned absolute-path tools, not on this PowerShell session's PATH. Blender, Godot, Unity, and Unreal Editor are not available through `Get-Command`.
- No third-party asset packs are present in this run's `assets/` directory. Internet downloads are not required and no replacement raylib may be fetched. Generated images may be used as project-local textures and their invocation count will be recorded.

## Engine and production approach

| Option | Fit | Decision |
| --- | --- | --- |
| Unreal or Unity | Strongest path to commercial-grade rendering, editor-authored animation, physics, and content pipelines. Neither editor is available here, and installing or changing the pinned environment would violate the run boundary. | Rejected for this run. |
| Godot | Suitable 3D scene tools and physics, but it is not installed or pinned in this workspace. | Rejected for this run. |
| C++20 + local raylib 5.5 | Already built for this exact Windows x64 profile; provides an OpenGL 3.3 window, input, audio, custom meshes, and shaders with no dependency download. It requires custom game physics, models, and UI. | Selected because it is the only provisioned, permitted stack. Use authored meshes, a small custom lighting/fog shader, and conservative settings. |
| Blender-authored assets | Could provide real meshes and UVs, but Blender is not available through the current command lookup and no model files are present. | Not part of the pipeline; build the required silhouettes and components as source-defined meshes. |

This is a constrained native implementation, not a path to honest AAA photorealism. The goal is a cohesive, detailed, atmospheric 3D game with visible material response and functional gameplay, while reporting the fidelity gap accurately.

## Concept comparison (written before new gameplay code)

### A. Breakwater: High Water — selected

A rescue pilot returns to a storm-damaged coastal catchment occupied by an armed salvage crew. The central mechanic, **Gaugework**, lets the player change linked basin levels at manual sluice stations. The waterline changes which shore paths exist, wetness and mud change foot and tire traction, currents move debris, and exposed banks change enemy cover and sightlines. Tracks and impacts persist long enough to reveal routes. The pickup, fixed-wing aircraft, and helicopter give distinct ways to reach sites, but their use remains subject to road, runway, pad, and wind conditions. The player clears three control stations, restores the emergency beacon, and extracts the stranded crew.

**Strengths:** a legible, grounded world-state puzzle with direct effects on exploration, tactical choices, vehicle traction, cover, and evidence trails. It fits a compact terrain mesh and material-state simulation. **Risks:** water can look or behave like a switch; changing water must visibly animate and alter several gameplay systems, not merely recolor terrain. Aircraft are not watercraft, so the airfield remains on high ground and the gate mechanic affects their mission routes and approach choices rather than pretending it changes lift.

### B. Crosswind — rejected

A storm-coast ranger reads local wind corridors and opens or closes industrial shutters to route gusts. Wind affects bullets, vegetation, smoke, plane lift and crosswind landing, helicopter drift, and vehicle stability.

**Strengths:** all three vehicles and combat respond to one physical field; strong visual storytelling through moving grass and smoke. **Risks:** player-controlled wind routing is difficult to ground plausibly, and a convincing spatial gust field, ballistics, turbulence, and weather would consume most of this implementation and threaten stability/performance.

### C. Faultline — rejected

A mountain rescue crew uses a winch, anchors, and heavy vehicles to pull barriers, trees, and structures into new routes; the aircraft deliver rigging equipment while the helicopter handles suspended loads.

**Strengths:** tactile environmental interaction and clear vehicle roles. **Risks:** reliable constraint solving, breakable geometry, cable behavior, and persistent debris are substantially more complex than the pinned stack can support in this run. A brittle rope puzzle would undercut the required combat and exploration loop.

## First playable scope

- One compact coastal catchment, an accessible service airstrip, a main road loop, several heli pads, three sluice stations, and a rescue beacon.
- On-foot third-person action-adventure controls, a readable weapon, enemy patrol/alert/cover behavior, health/ammo, objectives, pause/settings, and a complete win/restart loop.
- Three operable vehicles available during normal play: a four-wheel pickup, a fixed-wing utility plane, and a helicopter. Each gets chase and cockpit views, enter/exit, collision response, vehicle-specific controls, component animation, and appropriate synthesized audio. The plane must take off and land; the helicopter must lift, hover, translate, and land.
- Gaugework must alter basin state, visible water boundary/current, accessible routes, soft-ground response, debris, enemy positions/cover, and persistent track evidence.
- Graphics settings must expose Low and High presets. Low targets integrated graphics with modest view range, shadows off, and capped particle density; High enables the full shader, more scene detail, and higher render settings. No locally unsupported quality is claimed as verified.

## Quality checklist and evidence plan

- [x] Three-concept comparison and stack constraints documented before new game code.
- [x] Current source compiled and linked with the pinned MSVC/C++20/raylib Release profile using `build.ps1`, producing `build/bin/Release/game.exe`; the compiler and profile were not changed. PE machine is x64; current hash and size are in `results.json`.
- [x] Launched the actual game, captured and inspected title, controls, settings, operation, driving, cockpit, flight, and helicopter screenshots under `screenshots/`.
- [x] Tested title, controls, Low/High graphics setting, audio mute, pause, and operation restart on the final binary. The expanded narration package contains 16 WAVs; the task-tour runtime check loaded all 16 voices and 5 audio streams and triggered/captured all four objective cues. The settings QA screenshots document master fader, captions, spatial-panning and mix-profile changes/restoration. Physical speaker output remains unverified. [ ] Combat hit/damage, gate interactions/tide effects, beacon completion, and a full mission replay remain unverified.
- [x] Exercised pickup entry, acceleration, steering, braking, chase/cockpit camera, and exit. [x] Exercised plane entry, throttle, ground roll, takeoff, bank turn, descent, and a logged landing. [x] Exercised helicopter entry, collective takeoff, hover/translation, landing, and cameras. [ ] Collision damage, plane stall recovery, and rotor-wash interaction were not verified.
- [ ] Runtime evidence does not yet establish water-driven traversal, traction, cover, or persistent world change. These systems are implemented in source, but the mission was not played through to validate their combined effect.
- [x] Completed a visual review and refinement pass, then rebuilt and relaunched. Captures show a cohesive storm palette and generated surface textures; they also show conspicuously low-poly characters, vehicles, trees, structures, and flat water. The photorealistic/AAA quality gate fails.
- [x] `research/sources.csv` contains 551 distinct source IDs / exact URLs and 550 canonical pages: 450 direct page excerpts and 101 search-result excerpts. This passes the requested 512-source count but does not mean 512 complete-document reviews.
- [x] `results.json` records build metadata, QA evidence, screenshots, research access levels, and limitations. Usage and controlled performance metrics remain null. Seventeen actual image-generation tool invocations were made in this run.

## Final execution record

### Vehicle and audio refinement pass — 2026-10-23

- Replaced the plane's single ellipsoid fuselage with a smooth, longitudinally tapered 32-sided mesh and refined the cockpit camera aim for all three vehicles. The pickup, plane, and helicopter keep their source-built mechanical detail and vehicle-specific controls.
- Added distinct generated weapon reports, a layered rocket-explosion sound, material-selected impacts for missed shots, terrain/obstacle sample-based muffling, and two delayed echo taps. These are lightweight stereo approximations, not HRTF or convolution reverb.
- Added `scripts/capture_vehicle_views.ps1` and captured all six exterior/cockpit views from the running game window. The captures validate rendering and framing, not photo-realism; their low-poly materials and scenery remain a major gap.
- A full Blender or commercial-model authoring/import pipeline was unavailable in this run, and no third-party model packs were added.

- Final source build: `build/bin/Release/game.exe`; built from the configured CMake project through the pinned wrapper with the pinned MSVC x64 Release profile. The latest incremental source build reported 20 C4244 conversion warnings and no errors. A clean full rebuild warning count was not measured.
- A launch and UI smoke run of the final binary completed. The on-screen counter displayed 60 FPS at 1280x720 in captures; no controlled benchmark was performed, and the GPU inventory was unavailable. High-preset performance is unknown.
- Audio runtime QA originally logged briefing and pickup-control events and produced a genuine gameplay screenshot. The expanded `--qa-task-tour` path then loaded all 16 voice clips and 5 streams, entered the approach radius for West, East, South, and Beacon, logged each matching narration event, and captured four live caption frames (`qa_task_narration.log`, 20261023_195842). Captions/settings changes are also documented by settings screenshots. Physical-speaker listening quality was not measured. The voice is Windows System.Speech output from the local Zira voice, not studio recording; distribution rights for the pre-rendered voice were not independently confirmed.
- Sound is custom raylib stereo audio with procedural wind, score bed, and vehicle streams, state-driven engine/rotor tone, weapon and explosion effects, surface-specific impacts, narrator ducking, distance falloff, pan, approximate terrain/obstacle occlusion, and two delayed echo taps. HRTF, full acoustic geometry, and convolution reverb are not implemented; physical-speaker quality is unknown.
- Pickup test log recorded entry and exit and a maximum speed of 13.55 m/s. Plane test log recorded takeoff at 18.08 s and landing at 32.80 s, with a 22.59 m peak altitude and 56.06 m/s maximum speed. Helicopter test log recorded takeoff and landing, 7.27 m peak altitude, and 7.79 m/s maximum horizontal speed. These are short interactive QA sequences, not a campaign playthrough.
- Combat and mission state changes have no completed interactive verification: the final menu smoke ended with zero shots, hits, or operated gates. Do not treat the mission implementation as a verified complete gameplay loop.
- A dedicated combat attempt used scripted keyboard movement plus simulated mouse movement/clicks. Keyboard movement changed the scene, but raylib recorded zero shots after twelve click attempts; the process summary was `shots=0,hits=0,gates=0`. The player walked toward the basin edge rather than reaching the west crew. This is an input-injection limitation of this QA path, so it establishes neither combat success nor a runtime combat defect.
- The original root-level `Breakwater_1.png` was overwritten by an earlier F12 capture due to a path-handling defect. No recoverable copy was found in this run's allowed workspace. The new capture was also saved under `screenshots/`; the root file now contains the replacement image.
- A later interactive recheck found that the desktop automation path did not reliably deliver input: ordinary Enter attempts left the title menu active, and `SetCursorPos` returned false. Several older QA scripts also searched the entire append-only runtime log, which could match stale events. Those scripts now require events from the current launch. The failed rechecks and their screenshots are excluded; they do not establish a game-input defect. Earlier vehicle sessions still have gameplay frames and per-session motion summaries, including car 13.55 m/s, fixed-wing takeoff/landing, and helicopter takeoff/landing. Further interactive revalidation is unavailable through this input path.
- The last source build used the pinned wrapper and produced `build/bin/Release/game.exe` (1,405,440 bytes, SHA-256 `41F0BAA28F8F4AC70CA360109805B9787FC324D0762FCEC480B26583C0346645`). That executable launched during the task-tour run. Its last incremental build emitted no warnings; a clean full rebuild was not measured. QA-only script edits after that build do not affect the executable.
- The research register has 534 distinct source IDs and raw URLs. It records 441 direct page excerpts and 93 search-only excerpts; it is not a set of 534 full-document reviews.

## Current known risks

- The host GPU inventory was unavailable. The on-screen 60 FPS observation is not a controlled performance measurement; High preset performance remains unknown.
- Keyboard input injection was accepted by the game during QA, but simulated mouse movement and clicks were not recorded. Combat aiming/hits therefore remain unverified by this interactive path.
- The source-defined low-poly models, flat water, limited audio, and lack of rigged characters fall well short of the requested photorealistic/AAA quality. The final visual gate is not met.

## Final low-preset and narration verification — 2026-10-23

- Added low-preset distance LOD for the pickup beyond 72 m, fixed-wing plane beyond 82 m, and helicopter beyond 88 m. Close vehicle views retain the full source-built geometry; High preset thresholds and geometry are unchanged. Low preset grass draws 240 blades and the far-tree set uses 120 models. This reduces far-field work while preserving close-up vehicle detail.
- Made explicit numeric casts for the warnings in the edited world source. The pinned incremental build completed with no warnings and no errors. The executable is 1,408,512 bytes, SHA-256 `F30D2353FA9C3F4BB1F4EBF060C0E26772C4D587B79A06D05695AC451CF43511`. The build profile remains unchanged; a clean full rebuild warning count is not available.
- Reran `scripts/qa_task_narration.ps1` against that binary at 20261023_202603. Runtime logged mission start, all four task callouts, and completion with exit code 0; it loaded 16/16 voice clips and 5/5 streams and captured four live caption frames. The on-screen counter read 13 FPS in the initial West-gauge capture and 60 FPS in later East/Beacon captures. These are transient on-screen observations, not a controlled benchmark; High-preset and GPU performance remain unknown.
- Research register count was checked again: 534 rows, 534 distinct source IDs, 534 distinct raw URLs, with 441 direct page excerpts and 93 search-only excerpts. The register exceeds the 512-source threshold, but the search-only records are not direct page reads and the direct reads are excerpts, not full-document reviews.
- QA scripts that use normal desktop input now scope their event checks to the session they launch so old append-only log events cannot create a false pass. The desktop input sandbox still prevented a fresh manual vehicle run (`SetCursorPos` failed; simulated Enter did not start the mission). Earlier interactive vehicle screenshots and motion summaries remain the available evidence; combat and the full mission loop remain unverified.
- The visual gate still fails: actual captures show low-poly silhouettes, flat-water rendering, and limited environmental detail. The added LOD improves lower-preset rendering cost; it does not make these assets photorealistic or AAA quality.

## Follow-up audio and difficulty pass — 2026-10-23

- Expanded the synthesized coast bed so the surf/foam layer responds to basin height. The score changes its harmony with gauge progress and increases its pulse/tension layer with nearby alerted enemies. Car combustion/exhaust harmonics, fixed-wing prop/engine harmonics, and helicopter blade/turbine harmonics now follow the active vehicle's RPM/load parameters instead of relying on a fixed loop pitch. Vehicle loops are quieter in the pickup cockpit. Underwater sources/listeners select muffled one-shots and receive additional attenuation. The two-tap reflection queue now stays at or below 48 events.
- The fresh final-mix task tour loaded all 16 narrator WAVs and all 5 runtime streams, then logged West, East, South, and Beacon callouts in sequence before exiting successfully after 26.50 seconds. Earlier live-game QA captured four matching caption frames. The current pass confirms game audio playback state, not physical speaker/headphone output.
- Added Relaxed, Standard, and Veteran difficulty selection to Settings. The modes adjust enemy health, damage, accuracy, alert range, movement speed, and firing cadence. The pinned build compiles these paths, but the difficulty menu selection and balance have not been independently exercised through live input.
- The pinned build wrapper completed with zero incremental compiler warnings; a clean full rebuild warning count remains unavailable. Final executable: `build/bin/Release/game.exe`, 1,409,024 bytes, SHA-256 `2DD6BE09724DD1F976250F0C7C3C7379B1DA3F29D798977128BAA64F16A6F1C6`.
- The research register remains at 534 distinct raw URLs and 533 canonical pages, meeting the 512-source minimum. Access levels show 441 direct-page text excerpts and 93 search-result excerpts; this is not 512 full-document reviews.
- No new image-generation calls were made during that audio/difficulty pass; the cumulative count at that point was four. The photorealistic vehicle/world gate, physical audio-quality measurement, full mission loop, and recent normal-input vehicle retest remained open.

## Vehicle shell and material refinement — 2026-10-23

- Replaced the pickup's two broad body ellipsoids with a smoothed, 2,496-vertex UV loft and a separate 1,372-vertex roof shell; textured its wheel-arch shells. Replaced the helicopter cabin pair with a 2,340-vertex loft and its primitive tail boom with a 1,400-vertex tapered mesh. The existing 2,944-vertex custom plane shell now uses its own aircraft-skin map with repeated UVs. Added pressed tailgate ribs, hinges, plate, and reflectors. Dedicated generated v2 enamel/aluminium maps are used by the vehicle shells; the first maps remain in `assets/` as iteration history.
- Fixed a material-blend state leak found in the first live captures: the per-vehicle texture mix now resets to the plain-color path after each textured model, while terrain, road, and concrete retain their own full-texture sampling. Final High-preset captures at `20261023_212653` cover all six exterior/cockpit views and each process exited 0.
- The pinned wrapper rebuilt the executable without incremental compiler warnings. The resulting x64 executable is 1,418,752 bytes, SHA-256 `0954335D61D8CE0EA0F1CB4C2DED3340C82C5143663B8871E456FE8089F11519`. The final task-narration tour at 20261023_212759 loaded 16/16 voices and 5/5 streams, dispatched all four objective callouts, captured four live caption frames, and exited 0.
- The source register was rechecked: 534 distinct source IDs and raw URLs, 533 canonical pages, 441 direct page excerpts, and 93 search-only excerpts. It clears the requested 512-source count; it is not 512 full-document reviews.
- Visual QA shows smoother vehicle body forms, tailgate surface detail, and distinct material maps, but the models still look stylized and remain far below War Thunder / Forza Horizon-level fidelity. There are no scanned/reference-built vehicle meshes or authored normal/roughness maps. Physics and vehicle operation were not freshly retested during this appearance-only change; prior interactive motion evidence remains the only operation evidence. A Low-preset task-tour sample took 28.72 wall seconds, 7.953 process CPU seconds, and peaked at 286,392,320 bytes working set; this is not an FPS benchmark. High-preset performance and GPU usage remain unmeasured.

## Vehicle surface and cockpit refinement — 2026-10-23

- Added generated v3 skins for the pickup, fixed-wing aircraft, and helicopter, plus three tangent-space normal maps derived from the matching albedo luminance. `scripts/generate_vehicle_normals.ps1` contains the deterministic Sobel-derived conversion. The normal images add small-scale surface response in the shader, but they are not authored/sculpted PBR bakes and there are no separate roughness maps.
- Added a beveled rounded hard-surface cube mesh (486 vertices), denser High-preset airfoil skin tessellation (8 span by 16 chord steps), seam and fastener rows, 36 tread blocks per row / 108 raised tread pads per tire on High (20 per row / 60 on Low), ten spoke and lug wheel details, and finer pickup door shut lines. The helicopter's rotor linkages now turn with the main rotor; transparent swept sectors add subtle RPM-linked propeller and rotor blur without a cylinder artifact.
- Moved cockpit cameras toward the pickup driver seat and fixed-wing pilot seat, adjusted instrument framing for all three cabins, and reframed exterior QA views to fit the vehicle silhouettes.
- Built the final source with the pinned wrapper: zero incremental compiler warnings, x64 executable at `build/bin/Release/game.exe`. Nine fresh High-preset captures at `screenshots/Breakwater_VehicleShell_20261023_223047_*.png` were taken from the running game window: front and rear three-quarter exterior angles plus cockpit views for all three vehicles. All nine processes exited 0; front angles and earlier matching exterior/cockpit angles were inspected. This is rendering/framing evidence only; physics was not retested in this appearance-only pass.
- Captures show additional material breakup, wheel and airframe parts, and readable fixed-wing/pickup instruments, but the meshes, world scenery, characters, and water remain visibly stylized and far below the cited commercial-game fidelity. The photorealistic/AAA gate remains failed. Vehicle operation evidence is from earlier interactive sessions; collision damage, stall recovery, and rotor-wash interaction remain unverified.

## Vehicle fidelity refinement v4 — 2026-10-23

- Generated v4 pickup enamel, aircraft aluminium/enamel, and helicopter enamel albedos. Rebuilt their luminance-derived tangent maps at lower strength, added generated mipmaps/trilinear filtering, and separated matte-painted vs aircraft-metal shader response. These are material refinements; the normal maps still are not independent sculpt bakes and no roughness maps exist.
- Increased custom shell vertex counts to 5,328 (pickup body), 2,440 (pickup roof), 6,288 (plane fuselage), 3,888 (helicopter cabin), and 2,304 (helicopter tail). High airfoil tessellation is 10×20 span/chord steps; Low is 5×10. Added hinged ailerons/elevators that respond visually to bank/pitch, refined projector headlamp optics, and softened projected ellipsoid ground-shadow approximations.
- The final pinned build has zero incremental compiler warnings. Nine fresh High-preset game-window captures at `screenshots/Breakwater_VehicleShell_20261023_230738_*.png` cover front/rear three-quarter and cockpit views for all vehicles. All nine capture processes exited 0; shader and all three v4-derived vehicle normal maps logged ready. The plane-front QA pose is banked to expose control-surface deflection. This verifies rendering only; handling, takeoff/landing, combat, and mission physics were not retested in this appearance pass.
- Visual inspection confirms a cleaner material response and additional model definition, but these source-built vehicles and interiors remain stylized and substantially below Forza Horizon / War Thunder quality. The photorealistic/AAA gate still fails. The new ground shadows are an ellipsoid approximation, not a shadow map.
- Cumulative ImageGen use is 17 tool invocations and 16 project-local texture assets (four world/sky textures and twelve vehicle-skin iterations). Research remains 534 distinct IDs/URLs, with 441 direct excerpts and 93 search-result excerpts; that is a source-count pass, not 512 full-document reviews. Resource usage, GPU inventory, and controlled performance metrics remain unavailable/null.

## Vehicle model and material refinement v5 — 2026-10-23

- Rebuilt the pickup body and roof with softer super-elliptical cross-sections and higher mesh density: 8,512 body vertices and 4,672 roof vertices. The fixed-wing shell now has 10,048 vertices; helicopter cabin/tail have 6,208/4,080. These remain hand-authored parametric shells, not imported production meshes.
- Changed the car seat cushions/backrests from hard boxes to rounded upholstered forms with bolsters and stitching. Added rounded aircraft/helicopter seats and visible harness belts; moved cockpit cameras back/up and set cockpit FOV to 62 degrees.
- Added one generated roughness map per vehicle. The deterministic maps derive spatial variation from albedo luminance gradients and per-material base values; they are not measured or artist-authored PBR data. The shader samples the maps beside the luminance-derived tangent normals and uses separate automotive enamel, aircraft metal, and helicopter satin responses.
- Enabled 4× MSAA. Pinned build succeeded without incremental warnings. Nine new live High-preset front/rear/cockpit views at `screenshots/Breakwater_VehicleShell_20261023_233042_*.png` each exited 0; all nine were inspected. Runtime logged `world-shader-ready` and `vehicle-detail-maps-ready`. This is visual smoke QA, not a physics retest.
- The captures still show a stylized low-poly world and simplified vehicle interiors. AA and material breakup do not erase the clear difference from Forza Horizon / War Thunder fidelity; the photorealistic/AAA gate remains failed. Operation evidence remains from earlier interactive sessions.
- The updated Low-preset objective-tour sample at `resource_20261023_233358` exited 0 after 28.73 s, used 8.266 process CPU seconds, and peaked at 493,264,896 bytes working set (331,464,704 bytes private; 13 threads). GPU metrics were unavailable. This transient sample cannot establish frame-time performance or integrated-GPU suitability; High-preset performance remains unknown.

## Imported vehicle detail and storm-sky refinement — 2026-10-24

- Kept the three credited CC0 glTF vehicle files unchanged. Added model-bound draw-time components to the imported hero exteriors: drilled brake-disc faces and roof-rail retainers for the SUV; tapered wing rib seams, fasteners, and a leading-edge pitot tube for the plane; lower-cabin fasteners, side rescue hoist/fairlead/cable/hook, and an aerial placed on the helicopter tail boom. Placement was corrected against the GLB position accessor bounds after live screenshots exposed floating roof/aircraft/helicopter details.
- Corrected the glTF material UV transform handling for the plane and helicopter, separated pane alpha from embedded albedo alpha, applied tinted partial-opacity glazing with depth writes disabled, and loaded `assets/storm_sky_v2.png` for the sky and environment reflections. This is a custom OpenGL 3.3 material response, not a modern engine's full PBR pipeline.
- The pinned wrapper build succeeded with no incremental compiler warnings. The first sandboxed attempt was blocked by the Visual Studio SDK registry lookup; rerunning the unchanged wrapper with the required SDK-version registry read succeeded. Nine fresh High-preset live-game capture processes (rear three-quarter, front three-quarter, cockpit for each vehicle) exited 0; the first crops are `screenshots/Breakwater_VehicleDetail_20261024_050209_*.png`. Inspected the SUV front, plane front, helicopter front, and cabin views. These are appearance captures only; no vehicle handling or physics retest was done.
- The screenshots show the better cloud panorama and the corrected attached detail, but they also confirm the source assets and world are still stylized: planar water, angular vegetation, simplified dashboards, dark/flat vehicle shading, and visible low-poly structure. The user-requested photoreal/AAA fidelity gate remains failed; no claim of matching Forza Horizon or War Thunder is supported.
- Cumulative ImageGen use is 19 tool invocations; the workspace has 17 generated image assets (five world/sky and twelve vehicle paint iterations). The research register remains at 534 distinct source IDs and raw URLs / 533 canonical pages: 441 direct-page excerpts and 93 search-only excerpts, not 512 full-document reviews. Vehicle-operation evidence remains from earlier interactive sessions; High-preset performance, GPU metrics, physical audio quality, combat validation, and complete mission-loop verification remain unavailable or incomplete as recorded in `results.json`.

## Aircraft landing-gear articulation and final visual review — 2026-10-24

- Added fixed-wing wheel-spin inertia: the three wheels accelerate toward runway speed, coast down after liftoff, and reset cleanly at the runway start. The nose gear steers with rudder input while grounded and stays aligned in flight. This is source/build evidence; I did not perform a new manual takeoff/landing session after the animation change.
- Removed three offset auxiliary spheres found behind the SUV bumper during rear-view inspection. Rebuilt with the pinned wrapper, then launched nine new High-preset game instances for exterior and cockpit views. Camera distance and plane-front angle were iterated to keep complete models in frame while showing their close-up surfaces. Every capture process exited 0; latest crops are `screenshots/Breakwater_VehicleDetail_20261024_052909_*.png`.
- Visual review confirms the bumper floaters are gone and model accessories stay attached. It also confirms the largest weakness remains: the models, cockpits, lighting, and surrounding world are stylized and plainly below Forza Horizon / War Thunder fidelity. The fidelity gate remains failed; visual captures do not prove the new aircraft animation or revalidate vehicle physics.


## Vehicle material, rotor and airfoil refinement — 2026-10-24

- Kept the three licensed CC0 hero GLBs unchanged. Their double-sided material flags now map to the renderer: back-face culling is disabled for the hero meshes and the shader flips back-face normals. The vehicle shader retains the custom layered clear coat; this remains a lightweight OpenGL 3.3 renderer, not a full modern-engine PBR pipeline.
- Corrected the airplane propeller blur to the plane perpendicular to the shaft and the helicopter rotor blur to the horizontal rotor disk. This removes a dark ellipse across the airplane canopy and fixes the helicopter rotor-disk orientation.
- Replaced flat airplane control-surface cubes with capped, cambered and paint-textured 3D aileron/elevator surfaces plus a moving rudder patch, each rotating about its intended hinge axis. They render from flight-control state; no fresh flight operation was performed after this appearance change.
- Fitted additional SUV service hardware and refined QA framing to show the aircraft side profile. No player camera behavior changed.
- The pinned wrapper build succeeded; the incremental compiler emitted no warnings. Nine current High-preset in-game renderer captures (SUV, plane, helicopter: exterior/front/cockpit) all exited 0. The latest inspected images are `screenshots/Breakwater_VehicleDetail_20261024_062002_*.png`.
- The mesh/surface work improves detail and corrects visible geometry/render issues, but the models, cockpits and world remain stylized. The photoreal AAA / Forza Horizon / War Thunder quality gate remains failed. Handling and complete gameplay were not re-tested on this appearance build.
- An official CC BY 4.0 Artec scanned rescue-helicopter model was considered but not imported: its download host did not resolve from the restricted environment. The 535-row source register records this investigation; the delivered asset directory still contains only the existing vehicle models.

## Additional vehicle material and cockpit refinement — 2026-10-24

- High now uses the run's 2K generated paint maps as the SUV paint, airplane white gelcoat, and helicopter main-enamel diffuse textures while retaining the original CC0 GLBs and their other authored materials.
- Cockpit camera offsets were iterated against the source model seat and panel coordinates. The High-preset helicopter pilot view keeps its separate runtime interior because the imported cabin shell obscures the pilot-facing view.
- Final visual QA set `screenshots/Breakwater_VehicleDetail_20261024_064402_*.png`: nine fresh High-preset processes, all exit 0. This is launch/render evidence only; vehicle operation was not retested.
- NASA's official RAVEN page was reviewed as another glTF aircraft candidate, but its experimental eVTOL geometry does not match this conventional rescue prop plane. The scanned Artec candidate remains unavailable through its CDN in this environment. No replacement vehicle mesh was integrated.
- The source register now has 536 exact source IDs/URLs and 535 canonical pages, including 443 direct-page excerpts and 93 search-result excerpts. The 512-source count is met, but direct reads are excerpts, not full-document reviews.
- Honest target status remains unchanged: the vehicle shells, cockpit interiors and environment are visibly stylized, and remain far below photoreal AAA / Forza Horizon / War Thunder fidelity.

## Vehicle appearance continuation — 2026-10-24

- Refine the SUV paint with a new storm-slate metallic-enamel base-color tile and locally derived subdued normal/smoother roughness maps. Keep the aircraft and helicopter's authored 2K albedos, and use v8 surface maps for them.
- Add generated fine cabin-material tiles and apply them to the runtime seats, door panels, and cockpit trim. Continue to preserve readable cockpit glass and instrument displays.
- Replace the SUV's close-view straight recovery pins with closed, bumper-mounted tow eyes.
- Rebuild through the pinned wrapper. A default sandbox attempt again hit the known Visual Studio SDK registry read failure; the exact wrapper succeeded when rerun with the required SDK registry access. No incremental warnings were printed.
- Rendered final-build High-preset QA: nine standard rear/front/cockpit views across the three vehicles plus close SUV wheel and suspension views. All eleven capture processes exited 0. This pass did not revalidate handling or mission progression.
- Visual inspection: the SUV surface is modestly bluer and less coarse; the tow-eye geometry is more plausible at close range. The aircraft, helicopter, cockpits, glazing, and world remain visibly stylized and far below the cited commercial-game fidelity. The AAA/photoreal gate remains failed.
- Update the asset and QA records with 29 cumulative ImageGen invocations and 25 project-local generated images (including the current three); retain the 536-entry source register scope as excerpts/search excerpts, not 512 full-document reads.

## Vehicle material and measured-detail pass — 2026-10-24

- Queried the licensed GLB mesh/accessor bounds and fitted the SUV headlamp projector details inside the authored lens bounds instead of below the lamps. Added a reflective rescue stripe to the helicopter cabin side.
- Tested the aircraft and helicopter material overrides against fresh in-game captures. Restored their embedded authored 2048×2048 albedo maps; the airplane's navy accents remain visible. The SUV continues to use the generated 1254×1254 v6 body map because its authored paint image is only 64×64.
- Generated matching v6-derived SUV tangent-normal and roughness maps without overwriting prior v5 files; updated `scripts/generate_vehicle_normals.ps1 -Version v6` to generate only the car maps. The aircraft and helicopter continue to use their v5 derived detail maps over their embedded authored albedos.
- The pinned build wrapper succeeded without incremental warnings shown. Nine fresh High-preset exterior/front/cockpit game captures from the current executable all exited 0; all nine are registered in `results.json`. The exact-bounds headlamp view was visually inspected.
- This pass is visual-only. I did not re-run driving, aircraft takeoff/landing, helicopter handling, combat, or mission completion. The screenshots still show stylized assets and a simplified world; the Forza Horizon / War Thunder / photoreal AAA target remains unmet.

## Measured vehicle tread and rescue-marking refinement — 2026-10-24

- Inspected the SUV GLB tire accessors and aligned 78 shallow tread blocks per tire to the measured rubber bounds. The tread geometry shares the existing wheel spin and front steering transforms; the fallback pickup retains its separate 108 tread pads per tire.
- Repositioned the airplane's orange/reflective search-and-rescue bands from the fuselage-obscured wing root to the outer wing and reduced their depth so they sit close to the upper airfoil. Widened the helicopter's reflective cabin stripe. Tested darker glass, inspected the renders, and reverted that trial because the airplane canopy became too opaque while SUV glazing did not visibly improve.
- Final pinned wrapper build succeeded with no incremental compiler warnings shown. Nine fresh High-preset game-renderer processes exited 0 for exterior, front, and cockpit views of all three vehicles; final captures are `screenshots/Breakwater_LiveQA_car_1792826376814.png`, `screenshots/Breakwater_LiveQA_plane_1792826397326.png`, and `screenshots/Breakwater_LiveQA_heli_1792826419888.png` plus their six companion views.
- Visual inspection shows the attached tire blocks and outer-wing bands render, but the imported models, interiors, and low-detail world remain stylized and far below Forza Horizon / War Thunder / photoreal AAA fidelity. This remains an appearance pass only; handling, takeoff/landing, combat, and mission progression were not revalidated.
- The active SUV material is the generated 1254×1254 v7 paint with locally derived v7 normal and roughness maps. Cumulative ImageGen use is 23 tool invocations and 19 project-local image assets. The source register remains 536 exact source IDs/URLs; it meets the 512-source count, with 443 direct-page excerpts and 93 search-result excerpts, not 512 full-document reviews.

## Vehicle surface, rotor, and rescue-marking refinement — 2026-10-24

- Generated seamless v8 coating tiles for the SUV, airplane, and helicopter, then derived matching normal and roughness maps. The SUV uses its v8 base-color image because the source paint is only 64×64; the plane and helicopter retain their authored 2048×2048 albedos. These maps add surface response but do not replace the source geometry or materially change the silhouettes.
- Fixed the helicopter livery path: the hand-drawn side mark now resets the material diffuse tint, and the orange lower-cabin band is split into 12 pieces per side fitted to sampled cabin-shell cross-sections. Added 11 small panel fasteners per side. The first refit only exposed a thin edge; the final lower-panel version is visible in the renderer captures.
- Restored the helicopter's authored two-blade rotor silhouette through normal operating RPM. The previous 82% swap to an almost invisible translucent disk hid the blades; the disk now replaces them only above 97% RPM. A tiny neutral texture and matte graphite material keep the source's near-black rotor albedo from erasing the blade shape.
- Rebuilt with the pinned wrapper: Windows x64 executable is 1,673,216 bytes, SHA-256 `076122BE37F7CB6E9F08950018B0F1DF22A7660B6E8EC71CFA5AEB36E529CF36`; the incremental build emitted no warnings. Nine fresh High-preset in-game framebuffer captures (exterior, front, cockpit for each vehicle) all exited 0 and are recorded in `results.json`.
- Tried a darker glass tint/opacity adjustment, inspected fresh screenshots, and reverted it because the rendered change was not visibly beneficial. A desktop-window capture attempt was invalid because a foreground Codex/Settings window occluded the game region; it is retained and excluded from evidence. The internal framebuffer captures are the valid visual QA.
- Cumulative ImageGen use is 26 tool invocations and 22 registered local image assets. The research register remains at 536 unique source IDs/URLs: 443 direct-page excerpts and 93 search-result excerpts, not 512 full-document reviews.
- The rescue stripe and rotor are more legible, but current renders and cockpits remain visibly stylized and far below Forza Horizon / War Thunder fidelity. This was visual QA only; handling, takeoff/landing, combat, mission completion, and High-preset performance were not retested or measured.

## Coastal vegetation and dynamic water refinement — 2026-10-24

- Added procedurally assembled pine and broadleaf canopy meshes with varied branch angles, drooping boughs, textured bark/needles/leaves, and deterministic shape variation. High uses 320 main trees, 170 shrubs, and 190 grass tufts; Low uses 120, 64, and 78. Five shared geometry variants per species reduce the repeated silhouette, while tree and shrub instances remain frustum-culled. Wind and rotor wash affect foliage sway in the renderer.
- Cut the broadleaf trunk top to end inside the crown so it no longer rises above the foliage. Each of the five pine and broadleaf variants keeps its own branch spacing and length pattern; placed instance counts are unchanged.
- Added five generated vegetation albedo tiles and two calmer marsh-ground iterations. The current bindings use `tree_needles_coastal_v2.png`, `tree_leaves_coastal_v2.png`, `tree_bark_coastal_v1.png`, and `marsh_ground_v3.png`. These are color textures, not scanned or full PBR material sets. Terrain texture scale was widened and terrain tint blending reduced to improve ground readability.
- Adjusted branch counts and spacing and reduced foliage-cluster dimensions so the tree tiers overlap less uniformly. Replaced the sphere sky dome with a 96×48 custom seam-duplicated UV mesh, then created `storm_sky_v3.png` to soften its panorama edge transition; sampled mean RGB difference between the left/right edges fell from 22.6 to 9.3. A wrap artifact may still appear depending on view direction, so it is improved rather than proven absent.
- Added fragment-scale water ripple normals and increased the sky-reflection contribution. Current water rendering shows more distinct moving bands and brighter reflection response, while remaining a shader approximation without fluid simulation.
- Replaced the flat coastal-water surface with separate 96×96 Low and 160×160 High meshes and a custom shader for layered wind chop, approximate sky reflection, glints, tide-aware fragmented shoreline foam, and expanding wake rings around the selected rotor/vehicle source. The helicopter shoreline capture seeds rotor wash for rendering; it is not evidence of a live-flight wake test.
- The pinned wrapper built the final source successfully with no incremental warnings shown. Four fresh Low-preset internal framebuffer captures from that executable exited 0: forest, close tree, open coastal water, and seeded helicopter wake. The runtime log reported both coastal vegetation and dynamic water ready.
- The final-source Low-preset four-objective narration tour sample exited 0 after 33.25 seconds: sampled CPU time 29.562 seconds, peak working set 1,689,776,128 bytes, peak private bytes 1,584,181,248, and 15 threads. GPU utilization and memory were unavailable; this is not a controlled frame-time benchmark.
- Visual inspection shows more tree shape variation, corrected broadleaf crown coverage, calmer ground texture, and a more legible water surface, but the scene remains stylized; the sky panorama transition is still visible from some camera directions. This pass did not revalidate vehicle handling, combat, mission completion, or live rotor/vehicle interactions. Earlier vehicle operation evidence remains historical, and the photoreal/AAA fidelity gate remains unmet.
- Cumulative image generation is 39 tool calls and 33 registered project-local images. The research register remains 536 exact source IDs/URLs with 443 direct-page excerpts and 93 search-only excerpts; this is not 512 full-document reviews.

## Tree, vehicle and gameplay refinement — 2026-10-24

- Tightened the pine bough cushions and secondary lobes so the individual needle fans show through more clearly, widened the randomized needle spread, and compacted broadleaf canopy clusters while adding a thirteenth leaf blade per clump. The pine mesh budget remains 63,338 vertices and the broadleaf mesh is 62,708 vertices, each below the 65,535 limit for 16-bit indices; compile-time and runtime guards remain active.
- Increased High placement to 400 main trees, 285 shrubs, 580 grass tufts, and 780 ferns; Low stays at 140, 100, 200, and 280. Current-source High and Low forest, close-forest, and tree-detail captures each exited 0. More of the slope is covered, but the models remain angular and stylized, with visible texture repetition and no scanned tree set.
- Fresh image records: High forest `screenshots/Breakwater_LiveQA_forest_1792866840738.png`, High close forest `screenshots/Breakwater_LiveQA_forest-close_1792866365772.png`, High tree detail `screenshots/Breakwater_LiveQA_tree-detail_1792866375698.png`, and matching Low views `screenshots/Breakwater_LiveQA_forest_1792866850682.png`, `screenshots/Breakwater_LiveQA_forest-close_1792866385990.png`, and `screenshots/Breakwater_LiveQA_tree-detail_1792866396140.png`.
- The pinned wrapper rebuilt the Windows x64 executable without incremental warnings. Current size is 1,764,352 bytes and SHA-256 is `A7B3C970626A685036DB07A6550C1F25F68877587FF278BFA78453984129B628`.
- Current-binary car run `20261024_221900` logged entry, driving, steering/braking, and exit; the session summary reported 8.80 m/s maximum speed. Current-binary helicopter run `20261024_221921` logged takeoff, hover, translation/yaw, and landing; the summary reported 8.38 m/s maximum horizontal speed and 7.17 m peak altitude. These are automated input runs, not human-play evidence.
- The latest aircraft route logged takeoff at 32.12 s, controllable flight, descent, and a non-crash terrain touchdown at x=-17.5 m,z=418.3 m, altitude 1.3 m and speed 37.1 m/s, outside the runway footprint. The route script exited 1 because it requested a final summary before closing the game; the harness was repaired afterwards and its syntax parsed, but the repair has not been rerun. The separate 8.2 m runway fixture (`20261024_222402`) logged touchdown and braking.
- Updated combat input QA run `20261024_213120` logged 24 assault-rifle shots and zero hits, with zero gate operations. Desktop captures were invalid because the game window was occluded by the browser. Combat remains unverified; the screenshots are excluded from evidence.
- Current High tour `resource_20261024_222737`: 30.36 wall seconds, 35.25 sampled CPU seconds, 1,804,390,400-byte peak working set, 1,697,103,872-byte peak private memory, and 275 samples. Current Low tour `resource_20261024_222638`: 29.77 wall seconds, 15.922 sampled CPU seconds, 1,742,798,848-byte peak working set, 1,635,356,672-byte peak private memory, and 269 samples. Both logged task-tour completion and all four cues; GUI process exit codes and GPU utilization/memory are unavailable. These are process samples, not FPS measurements. Runtime logs identify Intel Iris Xe Graphics, OpenGL 3.3.0.
- Research register: 536 exact source IDs/URLs, 443 direct-page excerpts and 93 search-result excerpts. This meets the requested 512-source count but is not 512 full-document reviews. Cumulative image generation is 45 tool invocations including failed attempts, with 38 generated assets reported and 36 individually listed paths. Three successful ImageGen calls were made in the vehicle-material continuation.

### Current quality checklist

- [x] Pinned Windows x64 build succeeds; latest incremental compiler output has no warnings.
- [x] Current-binary car and helicopter inputs logged entry/exit or takeoff/landing events and measured movement.
- [x] Current-binary 8.2 m seeded airplane final-approach fixture landed on the runway and accepted braking input.
- [x] Current-binary combat fixture logged four hits from four shots, enemy return fire, and a takedown.
- [x] Current-binary scripted objective fixture operated three gauges and the beacon, logged rising/falling water, four narrator cues, and mission completion.
- [x] Latest High and Low forest-close renderer captures exited 0 and show the current canopy LOD and density pass.
- [x] Current Low and High resource tours completed; measured CPU and memory only.
- [ ] Normal takeoff-return airplane runway landing: the latest route harness ended in a water ditch before the runway; the seeded short touchdown does not satisfy this gate.
- [ ] Manual full mission, natural combat/aiming, replay, and difficulty balance.
- [ ] Controlled frame-time/FPS and GPU utilization/memory on Intel Iris Xe.
- [ ] Photorealistic AAA visual target; models and scenery remain visibly stylized/source-built.

## Continued vehicle refinement — 2026-10-24

- Added three ImageGen base-color tiles for the SUV, airplane and helicopter, then derived matching tangent-normal and roughness maps from those exact tiles. The maps are approximate luminance conversions, not authored/scanned PBR data; visual review shows subtle surface response, while the source meshes still read as stylized.
- Corrected the airplane and helicopter first-person camera eye/target offsets to align with the cockpit panels and horizon. Removed the procedural plane seat overlay that blocked the instrument panel; the imported exterior/cabin asset still contains seating.
- Adjusted airplane and helicopter glass tint/opacity so exterior glazing reads lighter and reveals more cabin. The rotor still crosses the helicopter front capture at that camera angle; the rotor is animated and physically remains in that position.
- Pinned Windows x64 build succeeded with zero incremental warnings. Final executable: `build/bin/Release/game.exe`, 1,764,352 bytes, SHA-256 `A7B3C970626A685036DB07A6550C1F25F68877587FF278BFA78453984129B628`.
- Six latest-binary renderer captures (front/exterior and cockpit for car, plane and helicopter) were captured and inspected; all six capture processes exited 0. These are renderer checks, not manual playthroughs.
- Current-binary car QA (`20261024_221900`) logged entry, driving, steering/braking and exit, reaching 8.80 m/s. Current-binary helicopter QA (`20261024_221921`) logged takeoff, hover, translation/yaw and landing, reaching 7.17 m altitude and 8.38 m/s horizontal speed. The latest aircraft route logged takeoff, controllable flight, descent and a non-crash terrain touchdown at x=-17.5 m,z=418.3 m, outside the runway footprint. The QA route currently fails its runway-landing gate; the revised script closes the game before reading its summary and checks runway bounds, but only its PowerShell syntax has been rechecked. The separate current-binary 8.2 m landing fixture (`20261024_222402`) logged touchdown and braking; it does not validate a normal route.
- Research register remains 536 exact source IDs/URLs: 443 direct-page excerpts and 93 search-result excerpts. This meets the 512-source count, but it is not 512 full-document reviews. Image generation adds three successful calls to the prior 42, including failed attempts: 45 total; 38 assets reported, 36 paths listed.
- Quality remains below the requested photoreal AAA target: SUV, aircraft, helicopter, interiors, lighting and world still appear stylized. The latest vehicle-map performance was not measured, and a normal runway landing has not passed.
- Current Low tour `resource_20261024_222638`: 29.77 wall seconds, 15.922 sampled CPU seconds, 1,742,798,848-byte peak working set, 1,635,356,672-byte peak private memory, and 269 samples at 100 ms. High tour `resource_20261024_222737`: 30.36 wall seconds, 35.25 sampled CPU seconds, 1,804,390,400-byte peak working set, 1,697,103,872-byte peak private memory, and 275 samples. Both logged all four narrator cues; GUI exit code and GPU utilization/memory are unavailable. They are process samples, not FPS measurements.

## Latest refinement and verification — 2026-10-25

This section supersedes older “latest/current” status notes above. The historical entries remain as a record of earlier iterations.

- The vehicle appearance pass is complete for this refinement sequence: the SUV, prop plane, and helicopter now have generated surface maps, modeled hardware, moving control surfaces/rotors, and revised cockpit camera/interior presentation. The imported CC0 glTF base meshes remain stylized and below the requested photoreal AAA target.
- On the current vehicle-code build, the car input run `20261025_011239` logged entry/exit and a 8.77 m/s peak with steering/braking keys issued. Helicopter run `20261025_011305` logged takeoff and landing, reaching 7.26 m peak altitude and 8.41 m/s horizontal speed. These runs are scripted desktop input and renderer/runtime evidence, not human play.
- The aircraft route harness was retuned to coordinate pitch and bank and to track a runway glide path. Its latest run ended in `plane-water-ditching` near x=309.3 m,z=821.2 m before reaching the airfield. A separate current-binary short final-approach fixture (`20261025_011648`) landed at 2.98 s within the runway and applied 1.25 s of braking. A normal takeoff-return landing is still an open gate.
- Current-binary combat fixture `20261025_011225` registered 4/4 shots, enemy fire, and the enemy takedown. Mission fixture `20261025_011117` operated all three gauges and the beacon through the ordinary `Interact()` path, logged water rising/falling, loaded 16/16 narrator clips and 5/5 streams, captured four renderer frames, and exited 0. It teleports between tasks and disables enemies, so it is not a normal full playthrough or replay test.
- The vegetation pass enlarged textured pine and broadleaf canopy clusters, brightened foliage sky fill, moved the photographic cutout LOD to 28 m High / 22 m Low, and increased tree/shrub placement to 480/340 High and 160/120 Low. Grass and fern counts remain 580/780 High and 200/280 Low. Current High `screenshots/Breakwater_LiveQA_forest-close_1792879791442.png` and Low `screenshots/Breakwater_LiveQA_forest-close_1792879826174.png` renderer captures both exited 0. Midrange silhouettes are fuller; close crowns, ground and spacing remain stylized and visibly repetitive.
- Latest resource tours on the final executable: Low `resource_20261025_011411`, 59.91 wall seconds, 50.25 sampled CPU seconds, 1,786,245,120-byte peak working set and 1,687,228,416-byte private memory; High `resource_20261025_011519`, 60.57 wall seconds, 76.453 sampled CPU seconds, 1,804,120,064-byte peak working set and 1,706,958,848-byte private memory. Both completed the four-task tour. GUI exit code, FPS, GPU utilization and GPU memory were unavailable; the machine reports Intel Iris Xe / OpenGL 3.3.
- Latest pinned executable is 1,773,056 bytes, SHA-256 `DD5493BE4E70C429A53A1901C2748BBDA87498B1A83023E8F9D8F35EE2756B30`. Build wrapper completed without incremental warnings.
- Research remains 536 source IDs/URLs and 535 canonical pages, with 443 direct-page excerpts and 93 search-result-only excerpts. That meets the requested count of distinct sources, but it is not 512 full-document reviews. The project record reports 45 ImageGen invocations, 38 generated assets and 36 individually listed asset paths; no ImageGen calls were made during this final refinement pass.

### Current quality checklist

- [x] Windows x64 pinned build and launch smoke.
- [x] Vehicle input evidence: car and helicopter operations; seeded airplane runway touchdown.
- [x] Combat damage/takedown and scripted objective/beacon progression.
- [x] High/Low forest-close renderer evidence and Low/High process resource samples.
- [ ] Normal airplane circuit and runway landing.
- [ ] Manual full playthrough, natural combat aiming, replay and difficulty balance.
- [ ] Controlled FPS and GPU metrics.
- [ ] Photoreal AAA assets and animation quality; the current art direction is still stylized.

## Vehicle material and world-detail continuation — 2026-10-25

This entry supersedes the earlier “latest refinement” measurements above. All additions stayed inside the assigned run folder and kept the pinned MSVC 19.44 / C++20 / raylib 5.5 build profile unchanged.

- Generated fresh v11 base-color tiles for the SUV, plane and helicopter, then derived matching normal and roughness maps from their luminance. The CC0 glTF source meshes and detailed source materials remain in place; these maps add broad-shell finish response but do not turn the vehicles into scanned or measured PBR models.
- Continued the map pass after the vehicle work: High now places 720 main trees, 480 shrubs, 780 grass tufts and 1,120 ferns; Low places 240, 170, 260 and 400. Seven geometry variants per species are distributed through ten irregular grove centers. Full 3D tree models remain visible to 36 m High / 24 m Low before the generated cutout LOD.
- Changed the terrain tile to `assets/marsh_ground_v8.png`, widened its world tiling from 6 m to 9 m, and added restrained broad-scale soil color mottle in the shader. The terrain shader no longer samples vehicle clear-coat/normal/roughness maps, and vertex-colored grass renders without the default white albedo overriding it.
- Switched the equirectangular sky to seam-processed `assets/storm_sky_v4.png`; the saved image has matching left/right edge samples. This reduces a wrap artifact in the source image, but does not establish physically based sky lighting or eliminate every possible scene seam.
- Detailed the service hangar with sliding-door seams, front channels and lights, corrugated-side ribs, roof seams/vents, a side service door and stowed rescue gear. The airfield layout and gameplay collision surfaces were not changed.
- Captured fresh High exterior/front/cockpit views for the three vehicles from the final executable; all nine capture processes exited 0. High and Low forest, close-forest, tree-detail and coastal-water framebuffer captures also exited 0. Visual review found fuller midrange canopy and calmer ground tiling, while the scene remains sparse in some openings and visibly stylized.
- Final pinned wrapper build succeeded with no incremental compiler warnings. `build/bin/Release/game.exe` is 1,789,952 bytes, SHA-256 `28BB63E32BD01A9CEEDC920BE37B29917F8807C55DD1E4F94AB1E15BA16ECCDE`.
- Current-build car input QA `20261025_023506` logged entry, movement and exit, reached 8.74 m/s, and left 20 tire-track segments. Helicopter QA `20261025_023542` logged takeoff and landing, reaching 8.06 m altitude and 8.36 m/s horizontal speed. The seeded short fixed-wing fixture `20261025_023643` touched down within runway bounds after 2.56 s and accepted 1.25 s braking. The full normal airplane circuit remains unverified; the latest glideslope route run ditched before the runway.
- Final-binary scripted objective/narration QA `20261025_023933` operated all three gauges and the beacon, logged four task cues and mission completion, loaded 16/16 narrator WAVs, captured four frames and exited 0. It teleports between tasks and disables enemies; manual full mission, natural combat, replay and difficulty balance remain unverified.
- Current Low resource tour `resource_20261025_022915`: 59.15 wall seconds, 67.562 sampled CPU seconds, 1,819,832,320-byte peak working set, 1,720,090,624-byte peak private memory and 531 samples. High tour `resource_20261025_023031`: 59.40 wall seconds, 76.094 sampled CPU seconds, 1,837,346,816-byte peak working set, 1,742,344,192-byte peak private memory and 538 samples. Both completed their four-objective tours. These process samples are not FPS/frametime data; process exit code, GPU utilization and GPU memory were unavailable.
- The source register remains 536 distinct source IDs/URLs (535 canonical pages), including 443 direct-page excerpts and 93 search-result excerpts; the count is not 512 full-document reviews. Five recent ImageGen invocations bring the run total to 50; the ledger reports 43 generated outputs and 40 listed project-local asset paths. One new ground iteration was generated but not integrated.
- Quality status remains partial: the ordinary plane return route did not pass; manually played mission/combat, high-refresh responsiveness, hardware FPS/GPU measurements, and physical speaker quality remain unverified. Vehicle, tree, character and building assets remain stylized and far below the requested photoreal AAA target.

### Current quality checklist

- [x] Pinned Windows x64 build from the unchanged wrapper, zero incremental compiler warnings.
- [x] Current-binary automated car entry/driving/exit and track evidence.
- [x] Current-binary automated helicopter takeoff, translation and landing evidence.
- [x] Current-binary seeded airplane runway touchdown with post-touchdown braking input.
- [x] Current-binary four-objective mission/narration fixture; 16 voices and all four task cues.
- [x] Current High/Low vegetation and coastal-water captures; nine current vehicle views.
- [x] Current Low/High process resource samples during completed four-objective tours.
- [ ] Normal takeoff-return airplane landing; latest full-route harness ditched before the runway.
- [ ] Manual full mission, natural combat, replay and difficulty balance.
- [ ] Controlled FPS/frametime and GPU utilization/memory on the Intel Iris Xe system.
- [ ] Requested photoreal AAA quality; current meshes, materials, animation and world remain visibly stylized.

## Grove understory distribution and final-binary refresh — 2026-10-25

- Pulled the existing shrub, fern, rock and fallen-timber placement radii closer to the ten authored grove centers. Instance counts and draw budgets did not increase; the change concentrates the current detail budget under the tree clusters instead of scattering it thinly across surrounding slopes.
- Rebuilt with the pinned wrapper. The Windows x64 executable remains 1,789,952 bytes; SHA-256 `2569781CB8F7CB0790C3061BA2A43C60654D5F83DEB0665CFFBEB6998D058CAD`. Incremental compiler output had no warnings.
- High and Low forest-close captures from the new executable exited 0: `screenshots/Breakwater_LiveQA_forest-close_1792889392306.png` and `screenshots/Breakwater_LiveQA_forest-close_1792889408392.png`. Current High and Low tree-detail captures also exited 0: `screenshots/Breakwater_LiveQA_tree-detail_1792890100880.png` and `screenshots/Breakwater_LiveQA_tree-detail_1792890119667.png`. Visual inspection shows fuller grove silhouettes, while the tree-detail view still exposes a broad repetitive hillside and stylized ground/tree surfaces.
- Current-binary deterministic combat fixture `20261025_030444` logged 4/4 hits, enemy fire, a takedown, two screenshots and process exit 0. Natural mouse aiming and broader tactical behavior remain unverified.
- Refreshed all nine current-binary vehicle exterior/front/cockpit captures; each process exited 0. Current-build car inputs logged entry, driving and exit at 8.83 m/s with 24 tire-track segments and 6 footprint marks. Helicopter inputs logged takeoff/landing at 8.36 m/s horizontal speed and 6.62 m peak altitude. The seeded airplane touchdown fixture landed within runway bounds at 2.56 s and accepted 1.25 s of braking. A normal takeoff-return circuit remains unverified.
- Current-binary mission fixture `20261025_025102` completed three gates and beacon, loaded 16 narrator clips and five streams, logged four objective cues, captured four renderer frames and exited 0; it teleports between objectives and disables enemies.
- Current Low tour `resource_20261025_025340`: 59.16 s wall, 67.422 sampled CPU s, 1,820,852,224-byte peak working set and 1,722,236,928-byte peak private memory. High `resource_20261025_025451`: 62.09 s wall, 78.047 sampled CPU s, 1,837,842,432-byte peak working set and 1,740,853,248-byte peak private memory. These are process CPU/memory samples, not FPS/frame time; GUI process exit and GPU metrics remain unavailable.
- The map detail redistribution uses existing geometry and images, so no ImageGen call was made in this pass. Run-level generation totals remain 50 invocations including failures, 43 output assets and 40 listed local paths. Research remains 536 distinct IDs/URLs (443 direct-page excerpts and 93 search-only excerpts), not 512 full-document reviews.

### Latest evidence checklist

- [x] Pinned Windows x64 build and current executable hash recorded.
- [x] Current-binary vehicle exterior/front/cockpit renderer captures for all three vehicles.
- [x] Current-binary automated car, helicopter and seeded short airplane landing sequences.
- [x] Current-binary deterministic combat damage, enemy response and takedown fixture.
- [x] Current-binary objective/narration tour and High/Low task-tour resource samples.
- [x] Current-binary High/Low forest-close and tree-detail captures after understory redistribution.
- [ ] Normal fixed-wing takeoff-return-runway circuit.
- [ ] Manual full mission, natural combat behavior, replay and difficulty balance.
- [ ] Controlled FPS/frame-time and GPU utilization/memory measurements.
- [ ] Photorealistic AAA visual target; assets remain stylized/source-built or generated/derived.

## Vehicle module damage and render alignment — 2026-10-25

- Added separate condition values for the pickup powerplant, steering and four tires; airplane powerplant, controls and airframe; and helicopter powerplant, flight controls, main rotor and tail rotor. Those values now feed throttle response, tire grip, steering authority, lift, drag, yaw authority and rotor lift. Vehicle reset restores the modules, while crash states let propellers and rotors wind down and damaged engines emit smoke.
- Matched persistent impact marks to the imported hero-model axes, raised render pivots and orientation. The capture fixtures now place car/helicopter marks on the side visible to their QA chase camera. `scripts/qa_scene_view.ps1` accepts impact-view modes for all three vehicles.
- The pinned Windows x64 wrapper succeeded with no incremental warnings. The executable is 1,802,240 bytes with SHA-256 `6CC99FA46AB64C9810B41E08A5A41745AB9670837326DE861FB99E4D6F08B079`.
- Current-binary car input run `20261025_033532` logged entry, driving and exit, reached 8.77 m/s, and recorded 20 tire-track segments and 7 footprints. Helicopter run `20261025_033614` logged takeoff and landing, reached 8.30 m/s and 4.83 m peak altitude. Airplane short-approach fixture `20261025_033739` touched down inside runway bounds at 2.57 s and accepted 1.25 s braking.
- Current-binary seeded car, plane and helicopter impact-view processes each captured a renderer frame and exited 0. Visual review confirms a car body scrape; the aircraft marks remain subtle at the wide chase-camera distance. These fixtures do not validate natural collisions or module failure during player-controlled flight.
- Vehicle operation QA is revalidated for the car and helicopter; the airplane evidence remains a seeded short landing. The normal airplane takeoff-return route is still unverified. Visual models remain visibly stylized and well below the requested photoreal AAA quality, so vehicle development and the overall goal remain partial.

### Current vehicle checkpoint

- [x] Pinned build after damage/transform changes.
- [x] Same-binary car entry, movement, exit, tracks and footprints.
- [x] Same-binary helicopter takeoff, translation and landing event.
- [x] Same-binary airplane seeded runway touchdown and braking input.
- [x] Current game-renderer impact captures for all three vehicle types.
- [ ] Normal airplane takeoff-return landing; manual vehicle damage response.
- [ ] Photoreal AAA vehicle appearance.
- [x] Resume tree/map refinements after the vehicle pass advanced.

## Final vehicle damage visibility pass — 2026-10-25

- Tuned the airplane engine-cowling scorch and helicopter cabin-skin scrape to sit on their visible model surfaces, then captured the car, airplane and helicopter impact states again. Each renderer process exited 0. In the wide captures the pickup scrape reads; the two aircraft marks are still subtle and the models remain stylized.
- Re-ran controls after this exact executable build. Car run `20261025_034217` logged entry/driving/exit, 8.74 m/s, 24 tire tracks and 7 footprints. Helicopter run `20261025_034248` logged takeoff/landing, 8.42 m/s and 5.54 m peak altitude. Short airplane landing fixture `20261025_034344` touched down within runway bounds at 2.59 s and accepted 1.25 s braking.
- Latest executable: 1,802,240 bytes, SHA-256 `5AFFEF23CFA7F8C17AB1A4C86420F8A542FAF6F27B1BD2050DC4FE51F55CA692`. Pinned wrapper build exited 0 with no incremental warnings.
- `results.json` now records this exact build, the run IDs and current impact captures. Vehicle systems are revalidated for the scripted paths; normal airplane takeoff-return, manual collision damage, and photoreal quality are still open. The project remains partial; tree/map work was queued at this checkpoint and is advanced in the section below.

## Forest and ground-cover refinement — 2026-10-25

- Added two seeded foothill groves and expanded the map to 17 ground-cover centers. The original 15-grove tree placement sequence remains unchanged, avoiding a reshuffle of trees in existing roads, runway views and capture positions. The two new pockets add 168 High / 56 Low tree placement candidates.
- Increased the vegetation candidate budgets to High: 1,080 base trees, 800 shrubs, 1,350 grass tufts and 1,750 ferns; Low: 360, 280, 450 and 620. Tree candidates are additionally view-culled and rejected when terrain, road, gate, water or runway clearance fails.
- Added `assets/forest_floor_coastal_v1.png` as a blended coastal leaf-and-moss surface over the grove areas, fading at an 82 m radius. Existing vehicle, footfall, rotor-wash, blast and ambient-wind shader response is retained; vegetation bending is not a physical branch simulation.
- Rebuilt with the pinned wrapper. High and Low renderer captures for full forest, close forest and tree-detail views all exited 0. The new foothill line fills more of the ridge view, while the runway corridor remains open. The render still has stylized crowns and an intentionally open aircraft approach, so the photoreal AAA gate remains unmet.
- Current Low four-objective narration tour `resource_20261025_104346` completed in 60.59 s wall time with 65.203 sampled CPU seconds, 1,833,967,616-byte peak working set and 1,735,024,640-byte peak private memory. High `resource_20261025_104525` completed in 61.66 s with 77.469 sampled CPU seconds, 1,850,736,640-byte peak working set and 1,753,223,168-byte peak private memory. These are game-process measurements; exit codes, FPS/frame time, GPU use and GPU memory were unavailable.
- Current renderer screenshots: `screenshots/Breakwater_LiveQA_forest-close_1792917699707.png`, `screenshots/Breakwater_LiveQA_forest-close_1792917709916.png`, `screenshots/Breakwater_LiveQA_tree-detail_1792917592975.png`, `screenshots/Breakwater_LiveQA_tree-detail_1792917603353.png`, `screenshots/Breakwater_LiveQA_forest_1792918081462.png`, and `screenshots/Breakwater_LiveQA_forest_1792918092823.png`.
- [x] Rebuild and capture High/Low grove, close-grove and ridge views.
- [x] Sample High/Low process memory and CPU during the objective tour.
- [ ] Controlled frame-time/GPU measurement and manual forest traversal.
- [ ] Photoreal tree crowns, scanned/grounded PBR maps and physical plant response.

## Safehouse renderer pass and F-16A Netz approval gate — 2026-10-26

- Added a CC0 two-storey house shell with a project-authored hinged entry door, ground/upper-floor navigation boundaries, a 16-riser stair flight, balusters/handrails, two warm point lights, and CC0 sofa, desk and bed. Credits are in `assets/WORLD_ASSET_CREDITS.md`. The current room remains sparse; opaque window panels, repeated brick/surface patterns and plainly stylized geometry still fall far short of the requested high-fidelity world.
- The pinned wrapper build completed with exit code 0 and no incremental compiler warnings shown. Current executable: `build/bin/Release/game.exe`, 1,836,544 bytes, SHA-256 `9C7D5B649457BD0046D6AAF11ABB675E5AB01D809056FC6BC79F45C9D825502C`.
- Three High-preset renderer views from that executable captured safehouse exterior, ground-floor interior and upstairs; all capture processes exited 0. These are renderer fixtures, not interactive gameplay proof. A reliable Windows UI automation path could not initialize, so live door operation, floor ascent/descent and the full safehouse traversal remain unverified.
- F-16A Netz pre-production research is documented in `research/F16_NETZ_REFERENCE_AND_MODELING_PLAN.md`. Three generated mockups are saved under `research/f16-netz-mockups/`. The orthographic sheet is the primary silhouette proposal; the other two propose lighting/material presentation and close detail. No mesh has been started. The user explicitly requires mockup approval before 3D modeling, including a choice between standard Netz and War Thunder premium Netz (Mod).
- NAVAIR's generic F-16A factsheet lists 14.52 m length and 9.45 m wingspan, while the community-maintained War Thunder Netz (Mod) page lists 15.1 m and 9.4 m. The length discrepancy remains unresolved for the exact Block 10 configuration; 15.1 m is a provisional game-target reference, not a verified measurement. Blender is not discoverable on the current PATH; the user authorized necessary installs, so any Blender install will be confined to this workspace and only used if the approved modeling workflow needs it.
- `research/sources.csv` now contains 551 unique IDs and exact URLs / 550 canonical pages: 450 direct page excerpts and 101 search-result-only excerpts. This exceeds the requested 512 distinct-source minimum but does not represent 512 complete document reviews.
- ImageGen ledger updated to 60 invocations including failures, 53 reported image outputs and 50 project-local paths; the three F-16 review sheets are included in the local-path count.

### Current open gates

- [x] Rebuild the pinned Windows x64 executable.
- [x] Capture and inspect current High safehouse exterior, interior and upstairs renderer views.
- [x] Prepare F-16A Netz reference and mesh workflow brief before modeling.
- [x] User approved the orthographic standard-Netz mockup; Blender source and the integrated GLB are present.
- [x] Integrate detachable F-16 IR stores, lock/guidance state, live target interaction and cannon airframe damage.
- [ ] Raise the model to the approved mockup's photoreal detail bar; the current model remains visibly stylized.
- [ ] Refine all F-16 weapons toward documented real-world behavior; verify launch envelopes, seeker behavior, cannon ballistics, moving-target lead, stores, damage and dedicated sound in live gameplay.
- [ ] Calibrate whole-world physics against evidence: aircraft G-load, aerodynamic forces, stall/energy loss, wind/air density, ground traction, material response, suspension, and rotor wash. Current hand-tuned values are approximations, not validated real-world accuracy.
- [ ] Replace broad or generic collision responses with shape- and material-aware contacts across trunks, branches, terrain, buildings, debris and vehicles; include aircraft wing, gear and engine impacts, with persistent damage/effects where appropriate.
- [ ] Continue F-16 exterior/cockpit detail against the approved orthographic sheets, including small panel geometry, fasteners, control linkages, gear/well details, canopy readability, sensors, rails and deployed-store presentation; preserve a clear photoreal gate assessment.
- [ ] Verify reliable live safehouse traversal; the current input automation environment could not initialize.
- [ ] Requested photorealistic AAA quality; current game art remains visibly stylized and incomplete.

## F-16A Netz approved-model iteration — 2026-10-26

- The user approved the orthographic review and standard Netz. This supersedes the earlier no-model/approval gate above.
- Blender 5.2.3 was run from the project-local portable installation. The authored scene exports `assets/f16_netz.glb` with 112 meshes, 156 raylib primitives and 30 materials. The project contains the editable scene, reproducible builder, mesh-index map and generated header.
- Refinements include a lower-shell inlet aperture, dark duct, a reduced/darker pilot silhouette and smoked canopy, filled blue Stars of David, local M61 muzzle/scupper details, brake hydraulic hardware, speed-dependent surface maps and an additional generated paint-microdetail tile. The normal and roughness companions are locally derived approximations.
- Blender rendered and saved high-detail, top, side, nose and underside-intake inspection views. The intake shot showed a visible but narrow mouth at this angle; the cockpit silhouette and broad surface materials remain much less convincing than the approved photoreal mockup. This is a substantial authored asset pass, not a War Thunder fidelity match or a passed photoreal gate.
- The pinned `build.ps1` completed with exit code 0 after the first unchanged sandboxed build hit `MSB4184` during Windows SDK registry discovery. No compiler warnings were shown in the successful build output. The generated mesh IDs and new runtime detail texture are compiled into the executable.
- A scripted current-build F-16 weapon run acquired a moving rotorcraft, launched one wingtip AIM-9-class store, destroyed the target 1.34 seconds later, and exited 0 with one shot and one hit. A separate current-build fixture exercises the M61 against a rotorcraft; this is scripted code-path evidence, not a manual air-combat run. Both screenshots and exact log events are recorded in `results.json`.
- The two wingtip missile mesh ranges now have generated IDs and are hidden after release. The missile has a bounded seeker cone/range, terrain line-of-sight gate, airspeed interlock, turn limit, wind-relative drag, gravity, motor boost, timed life, exhaust trail and proximity effect. These are gameplay approximations, not a validated flight-dynamics or weapon-envelope simulation. Bespoke synthesized cannon and missile launch one-shots were added; physical speaker quality was not verified.
- Current in-game weapon screenshots are real renderer output from the deterministic fixture. They demonstrate HUD and aircraft rendering, not photoreal detail. The F-16 still fails the requested photoreal/mockup comparison; the opaque/black canopy issue remains unresolved and CUA initialization is still unavailable.
- ImageGen call count is now 64 cumulative, including this pass's one successful microdetail-tile generation. Model/token/cost usage metrics remain unavailable.

### F-16 current gates

- [x] Orthographic + standard Netz mockup approved.
- [x] Blender source model, texture pipeline, GLB and game mesh map produced.
- [x] Pinned Windows x64 project build succeeded with the integrated asset.
- [x] Scripted IR-missile lock, launch, target hit and ammo/store state executed on the game binary; target was destroyed and the process exited cleanly.
- [ ] Verify M61 cannon aircraft-target hit and damage on the current binary; first fixture timed out without a hit, so align and repeat instead of counting the gun as air-combat verified.
- [ ] Verify weapon handling in an interactive play session; current CUA startup path fails in this environment.
- [x] Render and inspect Blender high-detail, top, side, nose and intake views.
- [ ] Pass the mockup's photoreal material, cockpit and surface-detail bar; current model remains visibly stylized.
- [x] Capture and inspect a current-build aircraft frame with missile mode and the post-impact state.
- [ ] Revalidate in-game gear/speedbrake/afterburner animation and a normal flight/landing route on the final binary.

## User-requested realism and impact track — 2026-10-26

- [ ] Make the F-16's installed weapons functional and as close to documented real behavior as the game and available evidence allow; the current missile pass is an approximation and M61-to-air-target verification remains open.
- [ ] Improve the F-16 to the approved mockup's tiny-detail level, then compare the actual game render against the Blender sheet. Do not call the photoreal gate passed while the canopy/material and overall appearance remain stylized.
- [ ] Improve physics for every moving system (rigid-body momentum, G-force, aerodynamic lift/drag, air density/wind/weather, tire and foot friction by material, suspension, rotor lift/wash, impacts and damage) and verify with telemetry and interactive routes.
- [ ] Make world impacts depend on collision shape, material, speed, angle and mass. Prioritize accurate tree/trunk/branch hitboxes and aircraft tree strikes, then buildings, terrain, props, other vehicles, water, debris and repeated damage persistence.
- [x] Record what was executed, what was visually inspected and what remains unverified. No simplified harness or screenshot substitutes for manual control evidence.

### Scripted tree collision and F-16 weapon QA — 2026-10-26

- Rebuilt the pinned Windows x64 target with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1`. The unchanged sandbox attempt first failed at MSBuild Windows SDK registry discovery (`MSB4184`); the same wrapper then succeeded outside the sandbox. No toolchain pin was changed.
- Car/tree target test exited 0. Seed 1030 registered wood contact at 41.8 m/s, tree damage 1.00, tree felled, target-specific confirmation and an in-game screenshot. The first approach-normal implementation allowed the car to pass through the intended trunk; the normal now flips against the incoming velocity, and the retest confirms the intended tree.
- F-16/tree target test exited 0. The nose hit seed 94 at 60.0 m/s, registered wood/crown damage and entered the crash state. The capture shows the F-16 rendered in the forest, but the aircraft remains visually intact; impact breakup, wreckage and persistent debris are not implemented or verified.
- On the same executable, the scripted AIM-9 fixture exited 0 with one launch, one hit and target destroyed after 2.29 s. The M61 fixture exited 0 with five recorded hits and a rotorcraft kill. These fixtures do not establish natural aiming, realistic missile guidance/flight envelopes, ballistic dispersion or combat balance.
- Reviewed the actual current-build captures: `screenshots/Breakwater_1793022814440_1.png` (car/tree), `screenshots/Breakwater_1793022870547_1.png` (F-16/tree), `screenshots/Breakwater_1793022876269_1.png` and `screenshots/Breakwater_1793022877936_2.png` (AIM-9), and `screenshots/Breakwater_1793022882429_1.png` (M61). The car felling response works but the crown looks coarse; the F-16 remains below the approved mockup and War Thunder comparison, with an opaque-looking canopy. Continue visual and physics refinement.
- The tree collision changes use 896 seeded tree profiles and trunk/crown probes, but only selected tree collisions were tested. Other materials, obstacle shapes, weather effects, G-force response, repeated damage, and all vehicle/world crash combinations remain open. No human-play test was performed during this pass.
- No ImageGen call was made in this pass. Existing cumulative count remains 64 including failed attempts; token/cost usage is unavailable.

## F-16 canopy material mapping and cockpit render pass — 2026-10-26

- Rebuilt the approved-model header generator so authored GLTF material IDs account for raylib's additional runtime material slot. A startup log now confirms the canopy and HUD resolve to runtime material 7 (the old header expected 6); this was the cause of the opaque canopy in game. The translucent pass explicitly enables alpha blending.
- Replaced the generic aircraft dashboard in the normal F-16 cockpit view with the integrated GLB canopy frame, seat, pilot, HUD and instrument meshes. The pilot eye and camera basis now follow the aircraft's model-space position, pitch and roll. Added three flight-state readout overlays aligned to the modeled MFDs, plus an F-16 HUD cue.
- The pinned wrapper `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1` succeeded after the sandboxed SDK registry lookup was denied. Final incremental compilation showed no warnings. Current executable: `build/bin/Release/game.exe`, 1,878,016 bytes, SHA-256 `AD1CF0EC7825CBB00C746F231544DAFC78E2536D5C3D30EE2F4FFA69FB20CA21`.
- Current-binary `--qa-capture=plane-cockpit` and `--qa-capture=plane-front` processes both exited 0 and created `screenshots/Breakwater_LiveQA_plane-cockpit_1793033911435.png` and `screenshots/Breakwater_LiveQA_plane-front_1793033991765.png`. Both were inspected. The canopy now reveals the world and pilot in the rendered cockpit/chase views; the MFD faces and model-space dashboard are visible. The cabin and airframe still look stylized, the instrument area remains sparse, and the approved mockup/War Thunder fidelity gate fails.
- These are scripted camera fixtures, not evidence of a human-controlled flight, working displays under live input, or aircraft-system accuracy. Aircraft physics, weapon behavior, damage breakup, and interactive route validation remain open. No ImageGen call was made; cumulative count remains 64 including failed attempts. Token and cost usage remain unavailable.

## Safehouse glazing review — 2026-10-26

- The imported house GLB has separate clear-glazing meshes. Added a low-opacity render pass for the identified material (4 meshes, 424 vertices) and rebuilt with the pinned wrapper; the current build exits 0 with no incremental compiler warnings shown.
- Recaptured exterior, ground-floor and upstairs views with the current executable. All three capture processes exited 0. Visual inspection still shows pale, flat-looking panes rather than convincing transparent glass, so this pass is recorded as an implementation attempt, not a quality gate pass. Room dressing remains sparse and interactive traversal remains unverified.
- At that safehouse checkpoint, the executable was 1,839,616 bytes, SHA-256 `DE5918A16C06A9A513159B346F01F14F3DD07E6BD031A917D5A9551EAFE3A32A`. The later F-16 breakup build and hash are recorded below.

## F-16 tree-breakup continuation — 2026-10-26

- Added a persistent, side-specific breakaway wing assembly to the approved F-16 mesh path. The damaged aircraft continues with residual momentum, falls under gravity and drag, and settles against terrain; the detached authored wing keeps its own initial momentum, rotation, drag and ground slide. The detached assembly retains the visible wing, control surfaces, LEX, pylon, launcher, missile, marking and navigation light. A broken spar/rib treatment appears at the wing root. Tree crown damage leaves shortened branches and snapped stubs.
- The seeded current-build fixture struck the crown of tree seed 910 with the F-16 nose at 61.3 m/s. The port wing separated at 29.3 m/s fragment speed while the fuselage retained 13.5 m/s. The fuselage contacted terrain at 1.19 simulated seconds; the wing fragment contacted at 2.04 seconds and stopped 18.1 m from the fuselage. The tree took 1.00 damage and was not felled.
- scripts/qa_f16_breakup.ps1 launched the actual Windows game window, required the tree impact, breakaway, both ground contacts and fixture confirmation in the runtime log, then recorded the fresh in-game screenshot. Process exit code was 0. I reviewed screenshots/Breakwater_1793035862128_1.png; it shows the wreck and detached port wing from a high, pulled-back camera. The image remains visibly stylized and does not approach the approved mockup or War Thunder target.
- The pinned wrapper build exited 0 without incremental warnings shown. build/bin/Release/game.exe is 1,890,304 bytes, SHA-256 8AECEBE7A82784198E4F7D382765FDE84AA9F8321CAF6CEE562870CC6EF38C81.
- This verifies one deterministic nose-to-tree-crown case and visible breakup integration. It does not validate realistic structural limits, general wing/stores separation, all tree contacts, weather-dependent aerodynamics, other material impacts, or a human-controlled crash. The breakup values are hand-tuned gameplay approximations.

### Current F-16 and impact gates

- [x] Capture an actual current-build tree strike, breakup, fuselage contact and wing-fragment contact; review the screenshot.
- [ ] Raise the F-16 model and in-game materials toward the approved mockup; the current silhouette, cockpit, surface treatment and scene remain stylized.
- [ ] Expand damage response across impact location, angle, speed, mass, stores, structures and terrain; calibrate thresholds against reliable aircraft evidence.
- [ ] Verify ordinary player-controlled flight, weapons, landing and impact routes; this fixture is scripted and does not establish natural gameplay.
- [ ] Continue shape- and material-specific collision work across trees, buildings, terrain, water, vehicles and debris.

## F-16 wing-surface detail continuation — 2026-10-26

- Extended the authored wing-detail mesh across the lower skin with the same five fine seam paths, three framed access covers per side, and two rows of flush fasteners. The details use aircraft-scale dimensions and stay in the existing combined detail mesh, so the model remains at 112 glTF mesh nodes and 156 runtime primitives rather than adding placeholder objects.
- Fixed the Blender text-to-mesh export naming so service stencils keep their authored mesh-data names and the generated runtime index ranges validate. The first export correctly stopped at that validation after saving its preview; the corrected export completed with exit code 0.
- Blender 5.2.3 regenerated the editable scene (16,998,788 bytes), GLB (14,779,700 bytes), mesh map and generated header. Five Blender inspection views rendered successfully. High-detail and underside/intake views were reviewed; the new geometry does not close the photoreal fidelity gap.
- The pinned Windows build exited 0 without compiler warnings. The rebuilt game rendered an F-16 front-view fixture (exit 0), then reran the tree-breakup fixture (exit 0). The latest seed 910 capture records the port wing fragment contacting terrain at 2.03 simulated seconds, 17.7 m from the fuselage; the fuselage contacts at 1.18 s. The airframe remains stylized in the actual game render.
- Current executable: 1,890,304 bytes, SHA-256 594C5460A55426040F9DFB5A7686C70576D377F67386D529582A3A576D6C6D1B. No ImageGen call was made; cumulative ImageGen count remains 64. FPS/GPU/resource measurements were not collected for this pass.

### Current visual and runtime gates

- [x] Regenerate and render the detailed F-16 scene; rebuild the pinned Windows executable.
- [x] Confirm the updated model loads in the game and the seeded breakup still runs.
- [ ] Match the approved photoreal orthographic mockup in the actual game renderer; this pass remains a modest surface-detail improvement, not a fidelity pass.
- [ ] Validate the full flight/weapon/landing loop and broader physical damage with interactive play and measured evidence.

## Per-wheel tire contact and thermal pass — 2026-10-26

- The car now samples friction, rolling resistance and wetness at each of its four wheel contact points. Tread condition combines tire damage, temperature and wear; surface abrasion differs for asphalt/concrete, gravel, rock, mud, grass and water.
- Fixed a units error in the axle friction-circle calculation: longitudinal acceleration is converted to axle force in newtons before capacity checks, and longitudinal force is clamped against each axle's contact capacity before lateral force is calculated. This makes low-friction surfaces and combined braking/cornering constrain the car.
- Added four tire temperatures with ambient/airflow/wet-surface cooling, slip-work heating and wear. The broad 55 C grip optimum and wear rates are game approximations, not calibrated to an identified production tire. The game summary now records per-wheel temperatures and wear so later QA can measure them.
- Added the QA mode --qa-car-tire-thermal. The current binary exited 0 and the fixture passed after 1.51 simulated seconds: maximum temperature 20.51 C and total tread wear 0.00008114 from a seeded high-slip route.
- Current-binary input QA run 20261026_212256 logged mission start, car entry and exit, 11.16 m/s peak speed, 20 tire-track segments, 8 footprints and 0.02 car damage. All tires cooled to 19.5 C during that short low-slip route; no measurable wear occurred there. The five frames are now captured through the game renderer. The earlier 20261026_211750 screen-copy images showed Codex/browser content and are explicitly excluded.
- Current-binary --qa-car-tree-impact exited 0. Tree seed 1030 was struck at 41.8 m/s, reached damage 1.00 and fell; confirmation logged at 0.15 simulated seconds.
- Reworked qa_vehicle_detail.ps1 to use the six built-in game camera fixtures. The current run 20261026_212856 created and visually inspected car, car-cockpit, helicopter, helicopter-cockpit, plane and plane-cockpit frames from the internal renderer. These are static model views, not vehicle-operation tests. A prior interactive script run had no enter-plane event; its plane-labelled shots are excluded.
- Added sources S552-S553 on temperature and wear extensions to tire-force models. The publisher pages were blocked to direct fetch (HTTP 403 and 429), so these rows are search-result excerpts only; they are not full-paper reviews.
- Current executable after this pass: 1,913,344 bytes, SHA-256 D80AEE8114AFC2FB7D64EE0170CB545C623FF888A82DD58AE9D3568F678592B5. No ImageGen call or new performance/GPU measurement was made. The renderer remains OpenGL 3.3 and the captured vehicle/environment art remains stylized below the requested Forza/War Thunder target.

### Car/vehicle gates

- [x] Sample four tire contact surfaces independently and apply material-dependent grip/rolling/wear effects.
- [x] Keep longitudinal and lateral tire-force capacity in consistent units.
- [x] Exercise high-slip heating/wear, current-binary car input, and car/tree collision fixtures.
- [x] Capture and inspect current-build exterior and cockpit views for all three vehicle types.
- [ ] Calibrate thermal, wear, pressure and force curves against measured tire data.
- [ ] Reach the requested photoreal vehicle, cockpit and environment fidelity.
- [ ] Verify full normal player-controlled airplane takeoff, combat, return and landing on the final binary.

## Four-corner tire load-transfer continuation — 2026-10-26

- Replaced axle-aggregate grip limits with four contact-patch normal loads. The sprung-mass approximation transfers load front-to-rear under longitudinal acceleration and across each axle under cornering; per-wheel load sensitivity modifies friction capacity, and each wheel gets its own longitudinal/lateral friction circle using its sampled surface, wetness, tire health, temperature and wear.
- Added deterministic fixture checks for static normal-load conservation, rearward transfer under acceleration, forward transfer under braking, and outside-wheel loading in a turn. The current pinned build passed: 18,632.6 N total static load; acceleration shifted the front/rear split to 8,919.8/9,712.9 N; braking raised front load to 13,463.3 N; a positive lateral case loaded the left side to 10,835.2 N versus 7,797.4 N on the right.
- The combined high-slip tire fixture exited 0 and passed after 1.54 simulated seconds: 20.73 C maximum tire temperature, 0.00008172 total wear, and 16.00 m/s peak speed. The refreshed x64 executable is 1,917,440 bytes with SHA-256 `E2D6EA9FD6911D6E5EE5F72F160634ABF4471B5C32B65E329FCC73766047F19D`.
- Interactive current-binary car route 20261026_214938 exited 0; it logged mission start, entry, driving and exit after throttle, steering, braking and camera inputs. Peak speed was 11.09 m/s, with 20 tire-track segments, 8 footprints and 0.02 damage. Five current in-game F12 screenshots were captured; four show the car/exterior or cockpit route, and one shows the foot approach.
- These calculations remain a deliberately compact handling model. Center-of-gravity height, front/rear roll-stiffness share, load sensitivity, tire-force curves and thermal/wear rates are fixed approximations rather than measurements from this SUV, its suspension or its tires. The fixture validates force-transfer direction and bookkeeping, not real-world calibration or a complete maneuver envelope.

### Four-wheel contact gates

- [x] Conserve static load and transfer it in the expected direction under acceleration, braking and cornering.
- [x] Apply each wheel's surface-dependent grip and combined longitudinal/lateral force cap independently.
- [x] Re-run the high-slip tire fixture and a current-binary interactive car input route.
- [ ] Measure and tune vehicle center of gravity, suspension, tire curves, temperature response and wear against suitable real vehicle/tire data.
- [ ] Test split-friction surfaces, combined braking/steering, high-speed transient handling, and repeated impacts across a broad material/weather matrix.

## Hostile road vehicle continuation - 2026-10-26

- Added two hostile road vehicles to normal mission startup. They patrol near the west/east gauge areas, can pursue, steer around sampled obstacles, respond to sampled ground friction and wetness, leave surface-dependent tire tracks, track with a roof gun and attack at a difficulty-scaled rate.
- Integrated hostile vehicles into player rifle, RPG blast and aircraft-cannon targeting. Damage is location-aware at a coarse level, can affect tires and steering, and creates a persistent smoking wreck. Player-car collisions apply a basic impulse and damage response.
- Current pinned Windows x64 binary exited 0 in the seeded 5.03 s enemy-vehicle fixture. The hostile truck drove 12.08 m and fired 11 shots, dealing 26.12 damage. Four player rifle rounds hit, reducing its health from 230 to 142. The first run had 0/4 player hits because of camera parallax; after the fixture aim was corrected, the repeat passed. Both results remain in the session log.
- Reviewed the real in-game renderer screenshots at screenshots/Breakwater_1793045361745_1.png and screenshots/Breakwater_LiveQA_hostile-road-vehicle_1793045722553.png. They show the enemy vehicle in combat and a side view of the pickup/world. The latter camera is a seeded capture and is not gameplay evidence.
- Current executable: 1,939,968 bytes, SHA-256 40E6562CF592DBA13592EBC721C3A2A33DFD230AF47895958CF54F0DF9249A90. The last pinned build showed no incremental warnings. No ImageGen calls were made; cumulative invocations remain 64. No current-pass FPS or GPU measurement was collected.
- This extends the mission encounter set, but does not close the quality gates. The vehicle model reuses the stylized rescue pickup with added turret geometry; its visual realism remains far below the requested target. Enemy steering, chassis contacts, probabilistic hitscan fire and damage are gameplay approximations. Natural AI encounter balance, full mission progression and manual play remain open.

### Hostile vehicle gates

- [x] Spawn hostile road vehicles in ordinary mission startup.
- [x] Add patrol/pursuit, steering, surface response, weapon fire, target hitboxes and localized coarse damage.
- [x] Validate a current-build player/enemy vehicle combat fixture and preserve the initial failed alignment result.
- [ ] Develop a dedicated, high-detail hostile vehicle model with an integrated, reviewed turret mount.
- [ ] Calibrate vehicle handling, collision and weapon behavior against appropriate measured data.
- [ ] Verify natural encounters and the complete mission loop through interactive play.
- [ ] Reach the requested photoreal vehicle and world quality; current captures remain stylized.

## Hostile turret render-origin refinement - 2026-10-26

- The static pickup capture exposed a floating roof gun. The detailed GLB body was translated from a ground-corrected render origin, while the add-on turret used the physics origin directly.
- The turret and beacon now use the same transform selection as the pickup body: ground-corrected for the detailed hero mesh, vehicle origin for the distant legacy LOD. This keeps the turret seated as the renderer changes detail levels.
- Rebuilt with the pinned Windows wrapper; exit code 0 and no compiler warnings were shown. Re-ran the seeded hostile-vehicle fixture on the rebuilt executable: 5.03 simulated seconds, 12 enemy shots, 20.90 damage, four of four player rifle hits, target health 230 to 142, and 12.06 m enemy travel. The fixture passed again.
- Reviewed screenshots/Breakwater_LiveQA_hostile-road-vehicle_1793046253101.png and screenshots/Breakwater_1793046295843_1.png. The static view shows the turret attached to the roof. The gameplay screenshot verifies the actual game renderer and encounter state, but the enemy model and setting remain stylized.
- Current executable is 1,939,968 bytes, SHA-256 18D64908E5FEFCDC5A8619B573CDBF98D682407EDCD99A9D7A0156E0BF6AE359. No ImageGen calls, controlled performance measurements or GPU readings were collected in this pass.

### Turret alignment gates

- [x] Use the detailed mesh's corrected render origin for the turret and warning beacon.
- [x] Verify the enemy-vehicle combat fixture again after the render correction.
- [x] Review a static in-game render and a gameplay capture.
- [ ] Replace the reused rescue pickup with a purpose-built, higher-fidelity hostile vehicle model.
- [ ] Improve the scene and vehicle materials to the requested photoreal level.

## Hostile fixed-wing combat continuation - 2026-10-26

- Added one hostile F-16 interceptor to normal mission startup. It patrols, pursues a player-controlled vehicle or foot target, uses a predicted gun solution, and fires difficulty-scaled 20 mm bursts.
- Its compact flight model applies thrust, lift, gravity, air-relative drag, wind, turn authority and bank response. Cannon rounds inherit aircraft motion, receive gravity and air-relative drag, and use swept terrain/tree/player collision. Player M61 and AIM-9 targeting now include the hostile fixed-wing aircraft.
- The latest pinned Windows x64 build exited 0 with no warnings shown. --qa-hostile-f16 passed after 7.51 simulated seconds: six cannon rounds, 78 damage, player health 1000 to 922, and 1,439.63 m fighter travel; the aircraft remained airborne. Seeded player M61 and AIM-9 fixed-wing target fixtures passed (9 shots to destroy; missile target destroyed in 2.13 simulated seconds).
- Inspected the actual game-renderer capture at screenshots/Breakwater_LiveQA_hostile-fixed-wing_1793048274243.png. It shows the project F-16 model and stormfield scene, which remain visibly stylized below the requested War Thunder/AAA target.
- Current executable: 1,959,936 bytes, SHA-256 25F5E0668174DB31A792EB4C782F3C0472392BC35BEE08D4B67BF9BDB13936E. No ImageGen calls this pass; cumulative invocation count remains 64. No controlled FPS, GPU, or usage measurement was collected.

### Hostile fixed-wing gates

- [x] Spawn the hostile fixed-wing aircraft in ordinary mission startup.
- [x] Add pursuit, target lead, cannon fire, projectile motion, and swept world/player contact.
- [x] Integrate player M61 and AIM-9 targeting against a fixed-wing target.
- [x] Pass the current-build seeded intercept and fixed-wing weapon fixtures.
- [ ] Validate natural encounters, human-controlled air combat, and the complete mission loop.
- [ ] Calibrate the aerodynamic, cannon, seeker, damage, and structural models against measured F-16/weapon data.
- [ ] Bring the in-game F-16, cockpit, and environment to the approved reference fidelity.

## F-16 tire and muted QA refresh - 2026-10-27

- Updated the approved standard-Netz Blender asset with a closed lathed tire profile, rounded shoulders, sidewalls and bead transitions. The regenerated GLB is 14,884,724 bytes, with 111 named source meshes, 161 raylib mesh primitives and 30 materials. The Blender source is 17,041,891 bytes.
- Rebuilt successfully with the pinned Windows x64 Release wrapper after the asset export. Executable: 1,963,008 bytes; SHA-256 `C3E0D274EC74AE5B7D97289678BA9F9D29DFE9F975829DEBC3627766B7290258D`. No compiler warning lines appeared in the successful build output. The initial sandbox attempt was blocked by Windows SDK registry access; the same wrapper then succeeded via the approved host path.
- All fresh QA launches used `--qa-silent` and logged `qa-audio-output-muted`. The seeded landing fixture passed at 73.10 m/s, with gear down, on the runway, and a −2.57 m/s sink rate. This does not prove a normal takeoff-return-landing circuit.
- Seeded F-16 checks passed: tree-crown strike at 61.3 m/s, port-wing separation, fuselage and fragment ground contact, six hostile cannon rounds/78 player damage, nine M61 shots to destroy a target, and an AIM-9 target kill at 2.13 simulated seconds. The hostile aircraft remained airborne, and the struck tree was damaged but not felled.
- Reviewed actual game-renderer captures from gear, close and top views, plus Blender renders. The revised tire reads as a solid sidewall assembly. The F-16 and game environment remain visibly stylized and do not meet the requested War Thunder/AAA fidelity gate. No controlled FPS or GPU measurement was collected.
- The research register contains 550 distinct canonical source pages, with 450 direct page excerpts and 101 search-result-only excerpts; it does not claim that all 550 pages were read in full. See `research/README.md` and `research/sources.csv`.

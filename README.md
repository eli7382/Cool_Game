# BREAKWATER: HIGH WATER

The white/red Porsche GT3 RS build is available on this branch. Run
`Play-Game.cmd`, start a mission and press E beside the Porsche at the base.
See [Porsche controls and model validation](docs/PORSCHE_FRONT_FIT.md).

**Breakwater: High Water** is a compact storm-coast action-adventure about reopening a flooded catchment and extracting stranded crews. Its central system, **Gaugework**, links three manual sluices to the basin's tide: operating a wheel raises or lowers water, changes which low routes are usable, alters ground traction, moves debris, and changes enemy cover and patrol positions.

## Operation

Clear the salvage crew around each gauge, reach the wheel, and use **E** to change the tide. Complete all three stations, then reach the beacon at the central tower and use **E** to finish the operation. The pickup, utility plane, and helicopter are available at the airfield and service pad. Footsteps, tire marks, impact particles, debris movement, wet-ground response, and rotor wash provide local feedback.

Combat offers a service pistol, pump shotgun, assault rifle, sniper rifle, and rocket launcher, with aim/spread, reloads, enemy suppression and approach behavior, health, and recovery. The F-16A Netz has its fixed 20 mm cannon and two detachable, IR-guided AIM-9-class wingtip missiles; moving rotorcraft patrols give the seeker live targets. The map, next objective, ammo, health, tide level, and vehicle instruments remain visible in play.

## Verification status

The latest pinned Windows x64 build completed with zero incremental compiler warnings. Current-binary car QA (`20261025_025215`) logged entry, driving and exit, reached 8.83 m/s, and recorded 24 tire-track segments; throttle, steering and braking inputs were issued, but stopping distance was not measured. Helicopter QA (`20261025_025240`) logged takeoff and landing, reached 6.62 m peak altitude and 8.36 m/s horizontal speed. These are automated input sequences, not human-play evidence.

The aircraft can take off and maneuver, but a normal return-route landing remains unverified. The latest full-route harness ditched before reaching the runway. The current-binary short final-approach fixture (`20261025_025325`) logged touchdown at 2.56 s inside the runway bounds and then 1.25 s of braking; it does not prove a normal takeoff-return-landing circuit. Vehicle exteriors and cockpits remain stylized rather than photorealistic.

The current F-16 weapon fixture (`--qa-aircombat`, 2026-10-26) acquired a moving rotorcraft IR target, launched the port store, and destroyed it 1.34 seconds after launch. It recorded one shot and one hit, then exited 0. A separate scripted M61-vs-rotorcraft fixture is recorded in `results.json`. These deterministic fixtures validate combat code paths, not human timing, physical audio output, or the weapon's full real-world envelope. The game uses an approximate guided-missile model and scripted patrol targets.

Current-executable combat fixture (`20261025_030444`) registered four hits from four shots, one enemy-fire response, and a takedown. The latest final-binary mission fixture (`20261025_025102`) used normal gauge/beacon interactions, completed all three gates, logged all four task narration cues, loaded 16/16 voice files, captured four frames and exited 0. It teleports between objectives and disables enemies, so it validates objective progression rather than a normal playthrough or replay. The latest High/Low forest-close, tree-detail and nine vehicle renderer captures are listed in `results.json`.

## Controls

| Input | Action |
|---|---|
| WASD | Move on foot; helicopter cyclic |
| Mouse | Look on foot; fine pitch/bank trim in the plane |
| Arrow keys | Look around on foot; plane pitch and bank while flying |
| Left mouse / F | Fire the selected weapon or the plane's fixed gun |
| V (while flying the F-16) | Toggle between M61 cannon and AIM-9 IR missiles |
| Right mouse | Aim on foot |
| 1–5 | Select pistol, shotgun, assault rifle, sniper rifle, or rocket launcher |
| Left Shift | Sprint on foot; aircraft speed brake |
| Space | Jump on foot; brake pickup; raise helicopter collective |
| Left Ctrl | Lower helicopter collective |
| W / S | Accelerate / reverse pickup; increase / reduce aircraft power |
| A / D | Steer pickup; lateral helicopter cyclic |
| Up / Down | Plane pitch |
| Left / Right | Plane bank |
| Q / E | Aircraft yaw; **E** also interacts or exits when grounded |
| E | Operate gauge/beacon; enter or exit a vehicle |
| C | Toggle the active vehicle's chase/cockpit camera |
| R / T | Reload on foot; T reloads in pickup/helicopter; R recovers the current vehicle |
| V | Toggle first-person view on foot |
| M | Mute/unmute synthesized audio |
| P / Esc | Pause/resume; Esc returns from menus |
| F11 | Toggle fullscreen |
| F12 | Save a screenshot under `screenshots/` |
| Up / Down, Left / Right, Enter | Navigate menus, adjust settings, and cycle difficulty |

A ground vehicle must be nearly stopped before exit. The plane must touch down, and the helicopter must descend near its pad before exit is allowed. Aircraft pitch, bank, yaw, throttle, and collective should be eased rather than held at full input. In the F-16, select AIM-9 with **V**, let the HUD acquire a rotorcraft inside the forward seeker cue, then fire with **F** or left mouse; missile launch is inhibited on the runway or below 277 km/h. The cannon fires a 500-round 20 mm belt and remains the default selection.

## Graphics and audio

The game starts on **Low**. Final Low tour `resource_20261025_025340` completed in 59.16 wall seconds, with 67.422 sampled CPU seconds, 1,820,852,224-byte peak working set, 1,722,236,928-byte peak private memory and 534 samples at 100 ms. High tour `resource_20261025_025451` completed in 62.09 wall seconds, with 78.047 sampled CPU seconds, 1,837,842,432-byte peak working set, 1,740,853,248-byte peak private memory and 559 samples. Both completed the automated four-objective tour; process exit code was unavailable. These are process-load samples, not frame-time or FPS measurements. Runtime identifies Intel Iris Xe Graphics at OpenGL 3.3.0; GPU utilization and memory were unavailable.

The Graphics & Audio settings page has Relaxed, Standard, and Veteran difficulty modes, separate master, effects, coastal ambience, music-bed, and narrator levels, output mute, subtitle and spatial-panning toggles, and Wide / Dialogue Focus mix profiles. Difficulty changes enemy health, damage, accuracy, awareness range, movement, and firing cadence; menu selection and balance were not freshly verified. The narrator covers briefing, all four objective approaches, gauge transitions, beacon transmission, contested objectives, defeat, vehicle controls, and safe exit/landing guidance. Captions are enabled by default, and voice playback ducks the other buses. Current High and Low task tours loaded all 16 narrator WAVs and five streams and logged West, East, South, and Beacon cues. The Low tour completion event was logged, but GUI process exit code was unavailable; four earlier live-caption frames document those objectives. Physical output quality remains unverified.

The raylib audio layer runs five streams for coastal wind, a synthesized score bed, and the three vehicle engines/rotor. Coastal surf density responds to the basin level; the score gains tension with nearby threats and rescue progress; and multi-harmonic engine/rotor synthesis tracks RPM, speed, and load. One-shots use distance falloff and stereo pan, terrain/obstacle occlusion and underwater muffling, material-specific impacts, and two delayed reflection taps with a strict echo cap. Narration ducks the other buses. This remains a custom stereo approximation: it has no HRTF, full geometry-based sound propagation, or convolution reverb, and its procedural loops are not recorded vehicle performances. The 16 bundled mono WAV narrator cues were generated with the locally installed Microsoft Zira Desktop en-US System.Speech voice. They load in-game without that voice installed; redistribution rights for the generated voice output were not independently confirmed. Task dispatch and playback-state timing were logged, but physical speaker/headphone quality was not measured. See `assets/README.md` for provenance details.

The look is a stylized storm coast, not photorealistic AAA. High uses CC0 1.0 Innerscene glTF hero models: a crossover SUV, single-engine prop plane, and light helicopter; source links and included files are listed in `assets/VEHICLE_ASSET_CREDITS.md`. The authored SUV paint image is 64×64. Current broad shells use generated v13 paint tiles with matching locally derived normal/roughness maps; aircraft and helicopter retain authored source materials for detailed small parts and livery. These response maps are luminance-based approximations, not sculpt-baked or measured PBR data. High's helicopter first-person view uses a separate runtime cockpit interior because the imported shell obscures the pilot-facing view. Cockpit camera offsets align closer to the modeled seats, panels and horizon; the airplane view avoids a duplicate seat obscuring the instruments. Aircraft control surfaces visibly follow pilot input. The three vehicles also have measured SUV tread/rescue hardware, aircraft seams and moving controls, and helicopter cabin fasteners, winch, cable, hook and animated rotor. Low and far-distance fallbacks remain source-built. 4× MSAA is enabled. The current executable has nine High-preset renderer captures for exterior, front and cockpit views across all three vehicles; all nine capture processes exited 0.

The latest forest pass uses seven reusable geometry variants per tree species, 17 ground-cover centers, and photo-cutout LOD at 36 m High / 24 m Low. The original 15-grove tree seed layout stays fixed; two independently seeded foothill groups add 168 High / 56 Low tree candidates. High uses 1,080 base trees, 800 shrubs, 1,350 grass tufts and 1,750 ferns; Low uses 360, 280, 450 and 620. A generated damp leaf-and-moss tile blends across grove floors with an 82 m radius while preserving road, gate and aircraft clearances. Current High and Low renderer captures for full forest, close forest and tree-detail views exited 0. The ridge canopy is fuller and the approach remains open; the slope and crowns still read as stylized, and branch sway is shader-based rather than physical simulation. The overall scene remains below photoreal AAA quality.

## Build and launch

From this folder in Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The project uses the benchmark's pinned C++20/MSVC and raylib 5.5 build. Keep this folder as the working directory so relative assets and output folders resolve.

The pinned Windows x64 build wrapper completed on 2026-10-26 with no incremental compiler warnings shown. `build\bin\Release\game.exe` is 1,839,616 bytes, SHA-256 `DE5918A16C06A9A513159B346F01F14F3DD07E6BD031A917D5A9551EAFE3A32A`. Three High-preset safehouse renderer captures from this executable exited 0. Existing separate QA records document car and helicopter operation and a seeded runway touchdown; the normal aircraft circuit and human-play session remain unverified. Launch it with:

```powershell
Set-Location 'C:\AI-Game-Benchmark\runs\luna-max'
& .\build\bin\Release\game.exe
```

The verified executable is at `build\bin\Release\game.exe`.

The pinned build completed without incremental warnings. The latest evidence includes a short seeded runway landing, a four-hit combat takedown fixture, a scripted three-gauge/beacon objective tour with all four narration cues, and High/Low process resource samples. A normal fixed-wing circuit and manual full-playthrough remain unverified. See `results.json` for the evidence boundary.

## Included materials

- `src/breakwater/` — game loop, world, gameplay, vehicles, and interface.
- `assets/` — generated environment and vehicle-detail textures plus the three CC0 glTF hero vehicle exteriors; see `assets/README.md` and `assets/VEHICLE_ASSET_CREDITS.md`.
- `screenshots/` — captures made from the running game during QA.
- `logs/` — build and event/QA records.
- `research/sources.csv` — 551 unique source IDs/URLs and 550 canonical pages, with 450 direct-page excerpts and 101 search-result excerpts recorded by access level.
- `DEVELOPMENT_PLAN.md` — concept comparison, stack decision, and verification checklist.
- `results.json` — run measurements, verification, provenance, and known limitations.

The three imported hero exteriors are CC0 1.0 models from Innerscene; sources and included-file details are recorded in `assets/VEHICLE_ASSET_CREDITS.md`. Safehouse and furnishing sources are in `assets/WORLD_ASSET_CREDITS.md`. No other third-party art, audio, or code pack is included.


## Vehicle mesh and rotor refinement — 2026-10-24

- The High-preset hero-model path now honors the imported glTF double-sided material flags, flips shading normals for back faces, and adds a layered automotive clear-coat response. The existing CC0 source GLBs are unchanged.
- Fixed the aircraft propeller and helicopter main-rotor blur planes so each is perpendicular to its actual shaft. The airplane ailerons, paired elevators and rudder are separate cambered, capped, paint-textured mesh surfaces that deflect from the flight-control values instead of using flat cubes.
- Added fitted SUV crossbars/lightbar mounts, side steps, recovery eyes and parking sensors; detail remains attached hardware rather than a replacement production mesh.
- At that earlier appearance pass, nine High-preset exterior/front/cockpit renderer views were taken from separate game processes and all exited 0. Those were visual checks only; the later final-binary vehicle QA results are summarized in the Verification status section above.
- The airplane QA exterior camera now shows a side profile. This changes benchmark framing only; the player camera is unchanged.
- A photogrammetry rescue-helicopter candidate was researched but not integrated because the official asset CDN hostname did not resolve from this environment. The current hero models remain the credited CC0 glTF files.
- Honest quality status: the updated models and surfaces still look stylized and below photoreal AAA / Forza Horizon / War Thunder fidelity. High-preset performance and the full mission loop remain unverified; see `results.json`.

## Safehouse and F-16A Netz

A CC0 two-storey house shell has an authored hinged front door, ground/upper floor navigation, a stair flight with rails, localized warm lights and three CC0 furniture pieces. Current High renderer captures exist for exterior, ground-floor interior and upstairs. They all exited 0; this verifies those renderer views only. The glazing now draws through a low-opacity pass, but the panes still look pale and flat; the furnishing/layout remains sparse. Interactive entry, stair ascent/descent and door handling are not revalidated on the current executable because the Windows input path is unavailable.

The user approved the orthographic mockup and standard Netz configuration. `assets/f16_netz.glb` is an original Blender-authored, metric-scale aircraft asset with 108 exported meshes, 154 raylib primitives and 30 materials. It includes a canopy/cockpit, pilot silhouette, IDF markings, movable control surfaces, retractable gear, speedbrakes, intake, nozzle, tanks and wingtip stores. The source scene and builder are `assets/f16_netz_source.blend` and `scripts/build_f16_netz.py`; the game uses the generated mesh map in `src/breakwater/f16_netz_mesh_ids.hpp`.

Blender renders for high-detail, top, side, nose and intake views are in `screenshots/F16_Netz_Blender_*.png`. The asset is substantially more detailed than the earlier primitive aircraft, but it still looks stylized beside the photoreal approved mockup and is below War Thunder-level fidelity. The body/cockpit, texture wear, and normal/roughness response are not photogrammetric or artist-reviewed. The pinned Windows build succeeded after the model was integrated. A fresh in-game F-16 screenshot was not obtained in this environment: the renderer process exited before writing a new screenshot or session log, and the desktop automation runtime failed to initialize. Earlier aircraft flight and seeded landing evidence remains in `results.json`; the asset pass does not add a new interactive flight validation.




$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resultsPath = Join-Path $root 'results.json'
$json = Get-Content -LiteralPath $resultsPath -Raw | ConvertFrom-Json

function Ensure-NoteProperty($Object, [string]$Name) {
    if ($null -eq $Object.PSObject.Properties[$Name]) {
        $Object | Add-Member -MemberType NoteProperty -Name $Name -Value $null
    }
}

Ensure-NoteProperty $json.qa 'high_preset_performance_note'
Ensure-NoteProperty $json.qa.low_preset_lod 'high_preset_grass_tufts'
Ensure-NoteProperty $json.qa.low_preset_lod 'high_preset_ferns'
Ensure-NoteProperty $json.qa.low_preset_lod 'low_preset_ferns'
Ensure-NoteProperty $json.qa.low_preset_lod 'low_preset_shrubs'

function Read-ResourceSample([string]$Path, [string]$Preset) {
    $sample = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if (-not $sample.task_tour_complete) { throw "Resource tour did not complete: $Path" }
    [pscustomobject]@{
        token = $sample.token
        graphics_mode = $Preset
        wall_seconds = $sample.wall_seconds
        process_exit = $sample.process_exit
        task_tour_complete = $sample.task_tour_complete
        sample_interval_ms = $sample.sample_interval_ms
        sample_count = $sample.sample_count
        cpu_seconds_sampled = $sample.cpu_seconds_sampled
        peak_working_set_bytes = $sample.peak_working_set_bytes
        peak_private_bytes = $sample.peak_private_bytes
        max_threads = $sample.max_threads
        gpu_metrics_available = $false
        log = ($Path.Substring($root.Length + 1) -replace '\\','/')
        scope = $sample.measurement_scope
    }
}

$exePath = Join-Path $root 'build\bin\Release\game.exe'
$exe = Get-Item -LiteralPath $exePath
$exeHash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$low = Read-ResourceSample (Join-Path $root 'logs\qa_resource_sample_20261025_025340.json') 'Low'
$high = Read-ResourceSample (Join-Path $root 'logs\qa_resource_sample_20261025_025451.json') 'High'

$taskLogPath = Join-Path $root 'logs\qa_task_narration.log'
$taskLogLines = Get-Content -LiteralPath $taskLogPath
$taskStampLine = @($taskLogLines | Where-Object { $_ -match '^timestamp=' } | Select-Object -Last 1)
$taskCaptureLine = @($taskLogLines | Where-Object { $_ -match '^screenshots=' } | Select-Object -Last 1)
$taskExitLine = @($taskLogLines | Where-Object { $_ -match '^process_exit=' } | Select-Object -Last 1)
if (-not $taskStampLine -or -not $taskCaptureLine -or $taskExitLine[-1] -ne 'process_exit=0') {
    throw 'Final mission/narration QA record is missing or did not exit successfully.'
}
$missionStamp = $taskStampLine[-1].Substring('timestamp='.Length)
$missionShots = @($taskCaptureLine[-1].Substring('screenshots='.Length).Split(',') | ForEach-Object { "screenshots/$_" })

$combatLogPath = Join-Path $root 'logs\qa_combat.log'
$combatLogLines = Get-Content -LiteralPath $combatLogPath
$combatStampLine = @($combatLogLines | Where-Object { $_ -match '^timestamp=' } | Select-Object -Last 1)
if (-not $combatStampLine) {
    throw 'Latest combat fixture record is missing or incomplete.'
}
$combatStamp = $combatStampLine[-1].Substring('timestamp='.Length)
$combatStart = [Array]::LastIndexOf([string[]]$combatLogLines,$combatStampLine[-1])
$combatRecord = @($combatLogLines | Select-Object -Skip $combatStart)
foreach ($required in @('shots=4','hits=4','enemy_fire=true','enemy_down=True','in_game_captures=2')) {
    if ($combatRecord -notcontains $required) { throw "Latest combat fixture record is missing $required." }
}
$combatShots = @('screenshots/Breakwater_1792890287018_1.png','screenshots/Breakwater_1792890287827_2.png')

$vehicleShots = @(
    'screenshots/Breakwater_LiveQA_car_1792889821475.png',
    'screenshots/Breakwater_LiveQA_car-front_1792889828138.png',
    'screenshots/Breakwater_LiveQA_car-cockpit_1792889835316.png',
    'screenshots/Breakwater_LiveQA_plane_1792889842423.png',
    'screenshots/Breakwater_LiveQA_plane-front_1792889849972.png',
    'screenshots/Breakwater_LiveQA_plane-cockpit_1792889857178.png',
    'screenshots/Breakwater_LiveQA_heli_1792889864240.png',
    'screenshots/Breakwater_LiveQA_heli-front_1792889871585.png',
    'screenshots/Breakwater_LiveQA_heli-cockpit_1792889879069.png'
)
$environmentShots = @(
    'screenshots/Breakwater_LiveQA_forest_1792887792925.png',
    'screenshots/Breakwater_LiveQA_forest-close_1792887805170.png',
    'screenshots/Breakwater_LiveQA_forest-close_1792887827223.png',
    'screenshots/Breakwater_LiveQA_tree-detail_1792888019842.png',
    'screenshots/Breakwater_LiveQA_coastal-water_1792887838238.png',
    'screenshots/Breakwater_LiveQA_forest-close_1792889392306.png',
    'screenshots/Breakwater_LiveQA_forest-close_1792889408392.png',
    'screenshots/Breakwater_LiveQA_tree-detail_1792890100880.png',
    'screenshots/Breakwater_LiveQA_tree-detail_1792890119667.png'
)
$newScreenshots = @($vehicleShots) + @($environmentShots) + @($missionShots) + @($combatShots)
foreach ($relative in $newScreenshots) {
    if (-not (Test-Path -LiteralPath (Join-Path $root ($relative -replace '/','\')))) {
        throw "Final screenshot is missing: $relative"
    }
}

$json.finished_at_iso8601 = [DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$start = [DateTimeOffset]::Parse($json.started_at_iso8601)
$json.elapsed_seconds = [int]([DateTimeOffset]::UtcNow - $start).TotalSeconds
$json.image_generation_calls = 50
$json.recent_image_generation_calls = 5
$json.image_generation_assets_reported_count = 43
$json.image_generation_asset_paths_listed = 40
$newAssets = @('vehicle_paint_car_v11.png','vehicle_paint_plane_v11.png','vehicle_paint_helicopter_v11.png','marsh_ground_v8.png')
$json.image_generation_assets = @((@($json.image_generation_assets) + $newAssets) | Select-Object -Unique)
$json.screenshots = @((@($json.screenshots) + $newScreenshots) | Select-Object -Unique)
$json.executable_size_bytes = $exe.Length
$json.executable_sha256 = $exeHash
$json.build_success = $true
$json.final_incremental_build_warning_count = 0
$json.build_note = 'The pinned Windows x64 wrapper built the latest vehicle-material and world-detail source successfully with no incremental compiler warnings. Current vegetation placements concentrate ferns, shrubs, rocks and fallen timber within existing grove centers without increasing instance counts. The executable hash and size are recorded above.'
$json.stack.asset_license_source = 'Project-local generated art assets; the run record reports 50 ImageGen invocations including failed attempts, 43 generated outputs and 40 individually listed project-local paths. Current vehicle, ground and sky maps are generated/processed color and derived response textures, not scanned or complete measured PBR sets. Vehicle exteriors use CC0 1.0 Innerscene glTF (credits in assets/VEHICLE_ASSET_CREDITS.md). Audio is procedural; sixteen narrator WAVs were synthesized with Microsoft Zira Desktop en-US using Windows System.Speech; redistribution rights for those recordings were not independently confirmed.'

$json.qa.resource_sample = $low
$json.qa.high_preset_resource_sample = $high
$json.qa.high_preset_performance_note = 'Final-build Low and High four-objective tours completed. Low: 59.16 s wall, 67.422 sampled CPU s, 1,820,852,224-byte peak working set and 1,722,236,928-byte private memory. High: 62.09 s wall, 78.047 sampled CPU s, 1,837,842,432-byte peak working set and 1,740,853,248-byte private memory. GPU, FPS and controlled frame time were unavailable.'
$json.qa.high_preset_performance_verified = $false
$json.resource_usage = [pscustomobject]@{
    cpu_time_seconds = $low.cpu_seconds_sampled
    peak_working_set_bytes = $low.peak_working_set_bytes
    peak_private_bytes = $low.peak_private_bytes
    gpu_utilization_percent = $null
    gpu_memory_bytes = $null
    source = 'Final Low process sampled every 100 ms during completed narration tour resource_20261025_025340; 59.16 s wall, 534 samples. FPS and GPU metrics unavailable.'
}
$json.low_preset_refinement = 'Final-build High and Low forest-close captures exited 0 after the understory redistribution. Low resource tour resource_20261025_025340 completed in 59.16 wall seconds with 67.422 sampled CPU seconds, 1,820,852,224-byte peak working set and 1,722,236,928-byte peak private memory. High tour resource_20261025_025451 completed in 62.09 wall seconds with 78.047 sampled CPU seconds, 1,837,842,432-byte peak working set and 1,740,853,248-byte peak private memory. Low places 240 trees, 170 shrubs, 260 grass tufts and 400 ferns; High places 720, 480, 780 and 1,120. Instance counts did not increase; the placement radii for shrubs, ferns, rocks and fallen timber now overlap existing grove interiors more closely. These are process samples, not FPS measurements; GPU metrics and process exit codes were unavailable.'

$json.qa.low_preset_lod.low_preset_far_tree_models = 240
$json.qa.low_preset_lod.low_preset_grass_blades = 260
$json.qa.low_preset_lod.high_preset_main_tree_models = 720
$json.qa.low_preset_lod.high_preset_shrubs = 480
$json.qa.low_preset_lod.tree_billboard_start_high_m = 36
$json.qa.low_preset_lod.tree_billboard_start_low_m = 24
$json.qa.low_preset_lod.high_preset_grass_tufts = 780
$json.qa.low_preset_lod.high_preset_ferns = 1120
$json.qa.low_preset_lod.low_preset_ferns = 400
$json.qa.low_preset_lod.low_preset_shrubs = 170

$veg = $json.qa.vegetation_qa
$veg.high_counts.main_trees = 720
$veg.high_counts.shrubs = 480
$veg.high_counts.marsh_grass_tufts = 780
$veg.high_counts.ferns = 1120
$veg.low_counts.main_trees = 240
$veg.low_counts.shrubs = 170
$veg.low_counts.marsh_grass_tufts = 260
$veg.low_counts.ferns = 400
$veg.shared_variants_per_species = 7
$veg.mesh_variant_model_count = 28
$veg.tree_3d_geometry_until_m = [pscustomobject]@{ High = 36; Low = 24 }
$features = @($veg.features | ForEach-Object {
    if ($_ -eq 'five shared geometry variants per pine and broadleaf species') { 'seven shared geometry variants per pine and broadleaf species' }
    elseif ($_ -eq 'High tree/shrub density 480/340; Low 160/120') { 'High tree/shrub density 720/480; Low 240/170' }
    elseif ($_ -eq 'photo-cutout tree LOD begins at 28 m High and 22 m Low') { 'photo-cutout tree LOD begins at 36 m High and 24 m Low' }
    else { $_ }
})
$features += @('Ten irregular grove centers concentrate the same draw budget into denser woodland clusters.','Shrub, fern, rock and fallen-timber placements now stay closer to grove interiors, filling the existing vegetation budget without adding instances.','Ground tile expanded to a nine-meter terrain repeat with low-frequency shader soil variation.','Terrain shading explicitly excludes vehicle clear-coat, normal and roughness response maps.','Vertex-colored grass uses the color-only shader path instead of the default white texture.')
$veg.features = @($features | Select-Object -Unique)
$veg.screenshots = @(
    'screenshots/Breakwater_LiveQA_forest-close_1792889392306.png',
    'screenshots/Breakwater_LiveQA_forest-close_1792889408392.png',
    'screenshots/Breakwater_LiveQA_tree-detail_1792890100880.png',
    'screenshots/Breakwater_LiveQA_tree-detail_1792890119667.png'
)
$veg.capture_process_exit_codes = @(0,0,0,0)
$veg.visual_assessment = 'Current-build High and Low forest-close and tree-detail frames show understory, rocks and logs concentrated closer to grove interiors without increasing counts. The tree-detail view still shows broad exposed hillside and ground repetition; tree silhouettes and the environment remain stylized and below the photoreal AAA target.'
$veg.limitation = 'Captures verify appearance only. Tree and grass motion is shader sway, not physical branch simulation. Textures are not scanned PBR maps; no controlled frame time or High/Low GPU measurement is available.'

$sky = $json.qa.sky_rendering_qa
$sky.active_panorama = 'assets/storm_sky_v4.png'
$sky.measured_edge_change = 'The v4 source panorama processor reports zero left/right RGB edge mismatch after wrap processing; the previous v3 image had a sampled mean edge difference of 9.79. This source-image measurement does not prove that every sky-dome camera angle is seam-free.'
$sky.residual = 'Panorama edge pixels now match in the saved image, but the procedural sky is not an HDR environment and the scene does not use physically based sky lighting.'
$sky.capture_process_exit_codes = @(0,0)
$sky.screenshots = @('screenshots/Breakwater_LiveQA_forest_1792887792925.png','screenshots/Breakwater_LiveQA_coastal-water_1792887838238.png')

$json.qa.vehicle_visual_capture_count = 33
$json.qa.vehicle_visual_capture_note = 'Thirty-three High-preset renderer views are registered across iterations. The latest batch uses the final executable and includes nine exterior/front/cockpit views for all three vehicles; all nine capture processes exited 0. Captures validate appearance only, not vehicle operation.'
$json.qa.vehicle_model_refinement = 'Latest appearance pass generated v11 base-color maps for all three imported CC0 hero vehicles and locally derived matching v11 tangent-normal and roughness maps. Current renderer frames show updated broad-shell finish and cockpit geometry; surface response is an approximation and the source meshes remain visibly stylized.'
$json.qa.latest_vehicle_interactive_retest = [pscustomobject]@{
    executable_sha256 = $exeHash
    car = [pscustomobject]@{ run='20261025_025215'; passed=$true; verified=@('entry','exit','driving movement','throttle, steering and brake inputs issued','8.83 m/s peak speed','24 tire-track segments'); footprint_marks=6; session_log='logs/breakwater-session.log' }
    helicopter = [pscustomobject]@{ run='20261025_025240'; passed=$true; verified=@('entry','takeoff','translation/yaw inputs','landing event'); logged_max_horizontal_speed_mps=8.36; peak_altitude_m=6.62; session_log='logs/breakwater-session.log' }
    airplane = [pscustomobject]@{ current_binary_normal_route_verified=$false; last_full_route='A previous controlled glideslope harness took off and maneuvered but ditched before the runway; the normal circuit has not passed.'; separate_fixture='20261025_025325'; fixture_landing_seconds=2.56; runway_touchdown_within_bounds=$true; braking_input_ms=1250; limitation='Seeded short final approach only; not a normal takeoff-return-landing route.' }
}
Ensure-NoteProperty $json.qa.latest_vehicle_interactive_retest.airplane 'normal_route_status'
$json.qa.vehicle_qa_evidence_note = 'Final-executable car QA 20261025_025215 reached 8.83 m/s and logged 24 tire-track segments after throttle/steering/braking inputs; stopping distance was not measured. Helicopter QA 20261025_025240 logged takeoff/landing and reached 6.62 m peak altitude and 8.36 m/s horizontal speed. Plane fixture 20261025_025325 touched down within runway bounds at 2.56 s and accepted 1.25 s braking. The normal route remains unverified.'
$json.qa.recent_interactive_qa_limit = 'Current-executable car and helicopter input sequences logged movement and landing operations. The airplane passed only a seeded short runway touchdown; the last full-route harness ditched before the runway. Input scripts are automated, not human play; stopping distance, realistic flight circuit and manual full mission remain unverified.'
$json.qa.plane_landing_fixture_run = '20261025_025325'
$json.qa.plane_landing_fixture_seconds = 2.56
$json.qa.plane_landing_fixture_process_exit_code = 0
$json.qa.plane_landing_fixture_note = 'Final-executable short fixture started 8.2 m above the runway centerline, logged touchdown at 2.56 s inside runway bounds (x=-9.1 m, z=106.2 m), then accepted 1.25 s of braking. This does not validate a normal flight circuit.'
$json.qa.quality_note = 'The latest pinned build passes current scripted car and helicopter operation sequences, a seeded short airplane runway touchdown, the four-hit combat/takedown fixture, a four-objective mission with all narrator cues, and current High/Low woodland and vehicle renderer captures. The normal airplane circuit remains unverified after the route harness ditched before the runway. Art remains stylized and below photoreal AAA; manual play, replay, difficulty balance, controlled FPS and GPU metrics remain unavailable.'

$json.qa.latest_audio_task_tour = [pscustomobject]@{
    run = $missionStamp
    log = 'logs/qa_task_narration.log'
    sample_log = $null
    voices_loaded = '16 of 16'
    streams_loaded = '5 of 5'
    objective_cues = @('west','east','south','beacon')
    task_tour_complete = $true
    wall_seconds = $null
    process_exit = 0
    caption_screenshots = 4
    note = 'Final-executable scripted fixture completed all three gauges and beacon, logged the four task cues, captured four game-renderer frames, and exited 0. It teleports between objectives and disables enemies.'
}
$json.qa.audio_task_tour_evidence = [pscustomobject]@{
    log = 'logs/qa_task_narration.log'
    run = $missionStamp
    voices_loaded = '16 of 16'
    streams_loaded = '5 of 5'
    objective_cues = @('west','east','south','beacon')
    live_caption_screenshots = 4
    process_exit = 0
    mode = 'final-executable scripted in-game mission fixture; auto-teleports between stations and disables enemies'
}
$json.qa.latest_combat_qa_attempt = [pscustomobject]@{
    run = $combatStamp
    log = 'logs/qa_combat.log'
    runtime_log = 'logs/breakwater-session.log'
    mission_started = $true
    fire_events = 4
    hits = 4
    gates_operated = 0
    enemy_fire = $true
    enemy_down = $true
    screenshot_status = 'Two current-executable game-window captures document the deterministic fixture, not art quality.'
    screenshots = $combatShots
    result = 'Current-executable deterministic burst fixture passed: 4/4 hits, enemy response and takedown. Natural aiming and broad tactical behavior remain unverified.'
}
$json.qa.combat_input_attempt.shots_recorded = 4
$json.qa.combat_input_attempt.hits_recorded = 4
$json.qa.combat_input_attempt.gates_operated = 0
$json.qa.combat_input_attempt.assessment = 'The current-executable deterministic fixture registered four hits from four shots, enemy return fire and a takedown. Natural mouse aiming, ordinary player movement during combat and combat balance are not established by this fixture.'
$json.qa.latest_vehicle_interactive_retest.airplane.normal_route_status = 'Not run on this render-only final build; previous full-route controller test ditched before runway. Normal takeoff-return landing remains unverified.'
$json.qa.mission_objective_progression_verified = $true
$json.qa.normal_mission_playthrough_verified = $false

$json.audio.narration_qa = @($json.audio.narration_qa) + "Final-executable mission fixture $missionStamp loaded all 16 narrator clips and 5 streams, logged West/East/South/Beacon cues, completed all three gauges and the beacon, captured four game-renderer frames and exited 0; scripted fixture, not manual play."
$json.quality_gates.gameplay = 'Partial. The final-executable mission fixture completed three gauges and beacon with narration, and current-executable combat registered 4/4 hits, enemy response and a takedown. Current car/helicopter input routes and a seeded short airplane touchdown passed; a normal airplane circuit, manual full mission, replay and difficulty balance remain unverified.'
$json.quality_gates.vehicles = 'Partial. The latest executable logged car entry, movement and exit (8.83 m/s), helicopter takeoff/landing (6.62 m altitude and 8.36 m/s horizontal speed), and a seeded short airplane landing with braking. The normal fixed-wing return route has not passed; vehicle models/cockpits remain stylized.'
$json.quality_gates.interactivity = 'Scripted gate interaction logged rising/falling water; the final mission fixture completed all three gates and beacon with four narration cues. Combat damage/takedown, track marks and current-build clustered understory rendering are evidenced. Natural traction/debris outcomes, physical vegetation response, rotor wash in live flight and ordinary mission pacing are not fully validated.'
$json.quality_gates.graphics = 'Fails the requested photoreal AAA target. The final map has a larger seamless ground tile, clustered tree placement, longer near-field 3D LOD and additional hangar details; vehicles, characters, trees, buildings and the rest of the world remain stylized/source-built or generated/derived, without scanned PBR sets or professional character animation.'
$json.quality_gates.reliability = 'Pinned Windows x64 build succeeds without incremental warnings. Final-binary combat, car/helicopter operation scripts, seeded short airplane touchdown, mission/narration fixture, and current High/Low woodland plus vehicle renderer captures passed. The normal airplane circuit and manual full-session/replay reliability remain unverified.'
$json.quality_gates.polish = 'Menus, HUD, settings, cameras, narrator coverage and stereo mix are implemented. Current vehicle and environment surfaces are more detailed, but normal runway return, full player progression, measured acoustic output and AAA asset fidelity remain open.'
$json.quality_gates.audio = 'Final-executable mission fixture loaded 16 narrator clips and 5 streams, logged West/East/South/Beacon cues, completed the mission fixture and captured four caption frames. Physical output quality, HRTF, full acoustic propagation and convolution reverb remain unverified.'

$known = @($json.known_limitations)
$known[1] = 'The latest full normal airplane return-route harness before the final render-only edits ditched before reaching the runway. The final executable passed a separate 8.2 m seeded final-approach landing and 1.25 s braking input; a normal circuit remains unverified.'
$known[2] = 'Vehicle input scripts are automated desktop input, not human play. The final car run reached 8.83 m/s and recorded 24 tire-track segments; helicopter reached 6.62 m peak altitude and 8.36 m/s horizontal speed. Detailed braking distance and human handling feel were not measured.'
$known[6] = 'The research register contains 536 distinct source IDs/URLs: 443 direct page excerpts and 93 search-result excerpts. It meets the unique-source count but is not 512 full-document reviews.'
$known[7] = 'Project record reports 50 ImageGen invocations including failed attempts, 43 generated outputs and 40 individually listed project-local image paths; model geometry and PBR quality are not implied by that count.'
$known[8] = 'Final Low/High tours sampled process CPU/memory during automated objective tours. GUI process exit code, controlled FPS/frame time, GPU utilization and GPU memory were unavailable on the Intel Iris Xe system.'
$json.known_limitations = $known
$json.notes = 'Latest pinned Windows x64 build succeeded without incremental warnings. The final executable passed automated car and helicopter operation sequences, a seeded short airplane runway touchdown, the 4/4 combat/takedown fixture, a four-objective mission with all narrator cues, nine current vehicle renderer views, current High/Low forest-close and tree-detail captures, and High/Low task-tour samples. Understory placement was pulled into existing grove interiors without increasing object counts. A normal airplane return route remains unverified after the controller test ditched before the runway. Models and scenery remain stylized and below AAA. Resource figures are process CPU/memory only; FPS/GPU metrics unavailable. Research has 536 source IDs/URLs (443 direct-page excerpts, 93 search excerpts), not 512 full-document reviews. ImageGen record: 50 invocations, 43 generated outputs, 40 listed project-local paths; token/cost usage unavailable.'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$serialized = $json | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($resultsPath, $serialized, $utf8NoBom)
$check = Get-Content -LiteralPath $resultsPath -Raw | ConvertFrom-Json
if ($check.executable_sha256 -ne $exeHash -or @($check.screenshots).Count -lt 240 -or @($check.screenshots | Where-Object { -not (Test-Path -LiteralPath (Join-Path $root ($_ -replace '/','\'))) }).Count -gt 0) {
    throw 'Final results verification did not match the executable or screenshot files.'
}
[pscustomobject]@{ executable_sha256=$check.executable_sha256; executable_size_bytes=$check.executable_size_bytes; screenshot_count=@($check.screenshots).Count; image_generation_calls=$check.image_generation_calls; generated_assets=$check.image_generation_assets_reported_count; listed_asset_paths=$check.image_generation_asset_paths_listed; mission_run=$check.qa.latest_audio_task_tour.run } | Format-List

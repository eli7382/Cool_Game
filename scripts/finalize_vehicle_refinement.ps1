$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resultsPath = Join-Path $root 'results.json'
$exePath = Join-Path $root 'build\bin\Release\game.exe'
$j = Get-Content -LiteralPath $resultsPath -Raw | ConvertFrom-Json
$exe = Get-Item -LiteralPath $exePath
$hash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$resourcePath = Join-Path $root 'logs\qa_resource_sample_20261023_233358.json'
$resource = Get-Content -LiteralPath $resourcePath -Raw | ConvertFrom-Json
$now = [DateTime]::UtcNow
$start = [DateTime]::Parse('2026-10-23T12:01:40Z').ToUniversalTime()
$stamp = '20261023_233042'
$modes = @('car','car-front','car-cockpit','plane','plane-front','plane-cockpit','heli','heli-front','heli-cockpit')
$latestShots = @($modes | ForEach-Object { "screenshots/Breakwater_VehicleShell_${stamp}_$_.png" })

$j.finished_at_iso8601 = $now.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'")
$j.elapsed_seconds = [int]($now - $start).TotalSeconds
$j.image_generation_calls = 17
$generationAssets = @($j.image_generation_assets)
foreach ($asset in @('vehicle_paint_car_v4.png','vehicle_paint_plane_v4.png','vehicle_paint_helicopter_v4.png')) {
    if ($generationAssets -notcontains $asset) { $generationAssets += $asset }
}
$j.image_generation_assets = $generationAssets
$j.stack.asset_license_source = 'Sixteen project-local OpenAI ImageGen textures (four world/sky and twelve vehicle-skin iterations); vehicle/world models are source-built and audio is procedural; sixteen narrator WAVs were synthesized locally with Windows System.Speech Microsoft Zira en-US. Redistribution rights for the rendered voice files were not independently confirmed.'
$j.executable_location = 'C:\AI-Game-Benchmark\runs\luna-max\build\bin\Release\game.exe'
$j.executable_architecture = 'Windows x64 PE (0x8664)'
$j.executable_size_bytes = [long]$exe.Length
$j.executable_sha256 = $hash
$j.final_incremental_build_warning_count = 0
$j.standard_output_path_latest_source_verified = $true
$j.build_note = 'Latest source build completed through the pinned wrapper with zero incremental warnings. The ordinary sandbox attempt was blocked by Visual Studio SDK registry lookup; the unchanged wrapper succeeded with the required SDK registry read. The executable launched in nine fresh High-preset vehicle visual captures, each process exiting 0. Full clean build warning count is unavailable.'

$screenshotRecords = @($j.screenshots)
foreach ($shot in $latestShots) { if ($screenshotRecords -notcontains $shot) { $screenshotRecords += $shot } }
$j.screenshots = $screenshotRecords
$j.qa.vehicle_visual_capture_count = 9
$j.qa.vehicle_visual_capture_note = 'Nine fresh High-preset game-window captures at 20261023_233042 cover rear and front three-quarter exterior views plus cockpit views of all three vehicles. The banked plane-front angle shows hinged control surfaces. All nine capture processes exited 0; world shader and the v4-derived normal and roughness maps logged ready. All nine final captures were inspected. They confirm rendering and framing, not photorealism or operation.'
$j.qa.vehicle_qa_evidence_note = 'Earlier interactive sessions logged pickup driving to 13.55 m/s, fixed-wing takeoff/landing (56.06 m/s maximum), and helicopter collective flight/landing (7.79 m/s maximum horizontal). The latest build launched for nine High-preset visual captures after a softened pickup shell profile, higher vehicle-shell tessellation, per-vehicle normal/roughness detail, rounded cabin seats, and 4x MSAA; physics was not retested in this appearance-only pass.'
$j.qa.resource_sample = [pscustomobject][ordered]@{
    token = $resource.token
    graphics_mode = 'Low'
    wall_seconds = $resource.wall_seconds
    process_exit = $resource.process_exit
    sample_interval_ms = $resource.sample_interval_ms
    sample_count = $resource.sample_count
    cpu_seconds_sampled = $resource.cpu_seconds_sampled
    peak_working_set_bytes = $resource.peak_working_set_bytes
    peak_private_bytes = $resource.peak_private_bytes
    max_threads = $resource.max_threads
    gpu_metrics_available = $false
    log = 'logs/qa_resource_sample_20261023_233358.json'
    scope = $resource.measurement_scope
}
$j.resource_usage = [pscustomobject][ordered]@{
    cpu_time_seconds = $resource.cpu_seconds_sampled
    peak_working_set_bytes = $resource.peak_working_set_bytes
    gpu_utilization_percent = $null
    gpu_memory_bytes = $null
    peak_private_bytes = $resource.peak_private_bytes
    source = 'PowerShell sampled the game process every 100 ms during Low-preset QA task tour resource_20261023_233358; GPU utilization/memory were unavailable.'
}
$j.qa.vehicle_model_refinement = [pscustomobject][ordered]@{
    latest_capture_set = $stamp
    graphics_preset = 'High'
    capture_modes = @('car rear three-quarter','car front three-quarter','car cockpit','plane rear three-quarter','plane front three-quarter','plane cockpit','helicopter rear three-quarter','helicopter front three-quarter','helicopter cockpit')
    capture_process_exit_codes = @(0,0,0,0,0,0,0,0,0)
    screenshots = $latestShots
    meshes = [pscustomobject][ordered]@{
        pickup_uv_body_vertices = 8512
        pickup_uv_roof_vertices = 4672
        helicopter_uv_cabin_vertices = 6208
        helicopter_uv_tail_vertices = 4080
        plane_uv_fuselage_vertices = 10048
        rounded_hard_surface_cube_vertices = 486
        high_airfoil_span_steps = 10
        high_airfoil_chord_steps = 20
        low_airfoil_span_steps = 5
        low_airfoil_chord_steps = 10
        tire_tread_pads_per_tire_high = 108
        tire_tread_pads_per_tire_low = 60
        wheel_spokes_per_tire = 10
    }
    vehicle_maps = @('vehicle_paint_car_v4.png','vehicle_paint_plane_v4.png','vehicle_paint_helicopter_v4.png')
    normal_maps = @('vehicle_normal_car_v4.png','vehicle_normal_plane_v4.png','vehicle_normal_helicopter_v4.png')
    roughness_maps = @('vehicle_roughness_car_v4.png','vehicle_roughness_plane_v4.png','vehicle_roughness_helicopter_v4.png')
    normal_map_method = 'Tangent-space normal maps derived from each v4 skin albedo luminance at strength 1.8 by scripts/generate_vehicle_normals.ps1; UV-derivative tangent basis and reduced shader strength. They are not independent authored/sculpted bakes.'
    roughness_map_method = 'Procedural grayscale maps from local albedo luminance gradients with base values 0.30 car, 0.31 plane, and 0.48 helicopter. They are not measured or artist-authored material data.'
    runtime_ready_events = @('world-shader-ready','vehicle-detail-maps-ready')
    material_response = 'Distinct glossy automotive enamel, aircraft-metal, and satin helicopter-paint response; vehicle-only color-space conversion; per-pixel generated roughness and tangent detail; mipmaps and trilinear filtering.'
    cockpit_detail = 'Car seat cushions, bolsters, stitching; aircraft and helicopter rounded seats with visible harness straps; interior cameras moved back/up and narrowed to 62 degrees.'
    antialiasing = '4x MSAA enabled at window creation; 9 live High-preset captures exited successfully. Performance effect on other GPUs is unmeasured.'
    articulated_surfaces = 'Ailerons rotate about their hinge in opposite directions with bank; paired elevators rotate with pitch. Banked plane-front capture shows this QA pose; no live control-input retest was made.'
    projected_shadows = 'Subtle terrain-projected ellipsoid shadows for car, plane, and helicopter; helicopter rotor-disk approximation at low altitude. Not shadow mapping or geometry-accurate cast shadows.'
    physics_retested = $false
    visual_assessment = 'High-preset captures show higher shell tessellation, softened pickup cross-sections, per-pixel material variation, rounded cockpit seats, refined pickup projector lamps, articulated fixed-wing surfaces, and 4x MSAA. Inspection still shows stylized geometry, simplified dashboards, flat lighting, and a visibly low-poly environment; the game remains far below photorealistic AAA / Forza Horizon / War Thunder fidelity.'
}
$j.quality_gates.vehicles = 'Earlier interactive sessions verified car acceleration/steering/braking, fixed-wing takeoff/landing, and helicopter hover/translation/landing. The latest build launched and rendered nine High-preset front/rear/cockpit views with super-elliptical pickup forms, higher tessellation, normal/roughness maps, detailed seats, and MSAA; physics was not retested in this appearance pass. The models remain stylized and far below commercial photorealistic fidelity.'
$j.quality_gates.graphics = 'Fails the requested photorealistic/AAA target. High-preset screenshots show UV-mapped vehicle materials, generated normal/roughness detail, wheel/airframe parts, cockpit instruments, and 4x MSAA, but the source-built vehicles, characters, vegetation, water, and scenery remain visibly stylized and low-poly.'
$j.low_preset_refinement = 'Distant vehicles use low-preset LOD while close-up vehicle models retain tread and component detail. Vehicle texture mipmaps use trilinear filtering and 4x MSAA is enabled. The latest build was checked with a separate Low-preset process sample; that transient task-tour sample is not a controlled frame-time or GPU benchmark.'
$limits = @($j.known_limitations | Where-Object { $_ -notlike 'Vehicle normal maps are derived from albedo luminance*' -and $_ -notlike 'Vehicle normal and roughness maps are derived*' -and $_ -notlike 'The latest six vehicle captures are visual-only QA*' -and $_ -notlike 'The latest nine vehicle captures are visual-only QA*' })
$limits += 'Vehicle normal and roughness maps are derived from albedo luminance/contrast rather than independent sculpt bakes or measured material data; no scanned/photogrammetry meshes or commercial vehicle assets are included.'
$limits += 'The latest nine vehicle captures are visual-only QA; handling, collision damage, stall recovery, rotor wash, and landing were not re-exercised on this appearance-refined build.'
$j.known_limitations = @($limits | Select-Object -Unique)
$j.notes = 'Model/effort and token/cost metrics remain unavailable and null. Seventeen actual ImageGen tool invocations were made; sixteen ImageGen texture assets are project-local (four world/sky and twelve vehicle-skin iterations); six normal/roughness maps are deterministically derived. Research register contains 534 distinct source IDs and raw URLs, 533 canonical pages, 441 direct page excerpts, and 93 search-only excerpts; these are not 512 full-document reviews. The latest build captured and inspected nine fresh High-preset vehicle views; all nine processes exited 0 and the vehicle shader plus derived normal/roughness maps logged ready. The latest pass changed vehicle shell shape/tessellation, cockpit seating/camera framing, per-pixel material response, control-surface visibility, and antialiasing, not vehicle physics. Prior interactive car/plane/helicopter movement and takeoff/landing evidence remains the operation evidence. The new Low-preset resource sample took 28.73 seconds and peaked at 493,264,896 bytes working set, but is task-tour scoped and not a controlled FPS/GPU benchmark; High performance and GPU inventory remain unavailable. The game remains stylized and does not meet the requested photorealistic AAA bar.'

$json = ConvertTo-Json -InputObject $j -Depth 100
[System.IO.File]::WriteAllText($resultsPath,$json,[System.Text.UTF8Encoding]::new($false))
Write-Output "Updated $resultsPath"
Write-Output "Executable bytes: $($j.executable_size_bytes)"
Write-Output "Executable SHA-256: $hash"
Write-Output "Screenshot records: $($j.screenshots.Count)"
Write-Output "ImageGen calls: $($j.image_generation_calls)"
Write-Output "Finished UTC: $($j.finished_at_iso8601)"

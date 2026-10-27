param([switch]$High)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $root 'build\bin\Release\game.exe')).Path
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$token = "resource_$stamp"
$graphicsMode = if ($High) { 'High' } else { 'Low' }
$arguments = @('--qa-task-tour',"--qa-task-token=$token",'--qa-silent')
if ($High) { $arguments += '--qa-high-preset' }
$stdoutPath = Join-Path $root ("logs\qa_resource_console_{0}.log" -f $stamp)
$stderrPath = Join-Path $root ("logs\qa_resource_console_{0}.err" -f $stamp)
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLogLength = if (Test-Path -LiteralPath $sessionLog) { [System.IO.File]::ReadAllText($sessionLog).Length } else { 0 }
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
$watch = [System.Diagnostics.Stopwatch]::StartNew()
$peakWorkingSet = 0L
$peakPrivateBytes = 0L
$maxThreads = 0
$maxCpuSeconds = 0.0
$samples = 0
try {
    while (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -lt 70) {
        $process.Refresh()
        if (-not $process.HasExited) {
            $peakWorkingSet = [Math]::Max($peakWorkingSet,$process.WorkingSet64)
            $peakPrivateBytes = [Math]::Max($peakPrivateBytes,$process.PrivateMemorySize64)
            $maxThreads = [Math]::Max($maxThreads,$process.Threads.Count)
            $maxCpuSeconds = [Math]::Max($maxCpuSeconds,$process.TotalProcessorTime.TotalSeconds)
            $samples++
        }
        Start-Sleep -Milliseconds 100
    }
    $process.Refresh()
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; throw 'Resource-sample task tour exceeded 70 seconds.' }
    $process.WaitForExit()
    $process.Refresh()
    $exitCode = $process.ExitCode
    $sessionText = [System.IO.File]::ReadAllText($sessionLog)
    $runText = if ($sessionText.Length -gt $priorLogLength) { $sessionText.Substring($priorLogLength) } else { '' }
    if ($runText -notmatch 'qa-task-tour-complete') { throw 'The four-objective narration tour did not log its completion event.' }
    if ($null -ne $exitCode -and $exitCode -ne 0) { throw "Resource-sample task tour exited with code $exitCode." }
    $watch.Stop()
    $result = [pscustomobject]@{
        token = $token
        executable = $exe
        graphics_mode = $graphicsMode
        process_exit = $exitCode
        task_tour_complete = $true
        wall_seconds = [Math]::Round($watch.Elapsed.TotalSeconds,2)
        cpu_seconds_sampled = [Math]::Round($maxCpuSeconds,3)
        sample_count = $samples
        sample_interval_ms = 100
        peak_working_set_bytes = $peakWorkingSet
        peak_private_bytes = $peakPrivateBytes
        max_threads = $maxThreads
        gpu_metrics = $null
        measurement_scope = "Game process only during the automated four-objective narration tour at the $graphicsMode preset; GPU utilization and dedicated GPU memory were unavailable."
    }
    $path = Join-Path $root ("logs\qa_resource_sample_{0}.json" -f $stamp)
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $path -Encoding utf8
    $result | ConvertTo-Json -Depth 4
}
finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}

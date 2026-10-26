$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$qaLog = Join-Path $root 'logs\qa_f16_breakup.log'
if (!(Test-Path -LiteralPath $exe)) { throw "Missing game executable: $exe" }
$priorLength = if (Test-Path -LiteralPath $sessionLog) { [System.IO.File]::ReadAllText($sessionLog).Length } else { 0 }
$start = Get-Date
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class BwF16BreakupQa {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
}
"@
$p = Start-Process -FilePath $exe -ArgumentList '--qa-plane-tree-impact' -WorkingDirectory $root -PassThru
try {
    $null = $p.WaitForInputIdle(10000)
    $p.Refresh()
    if (!$p.MainWindowHandle) { throw 'The F-16 tree-impact fixture did not create a game window.' }
    if (!$p.WaitForExit(20000)) { throw 'The breakup fixture did not finish within 20 seconds.' }
    $p.Refresh()
    if ($p.ExitCode -ne 0) { throw "The game exited with code $($p.ExitCode)." }
    $allLog = [System.IO.File]::ReadAllText($sessionLog)
    $runText = if ($allLog.Length -gt $priorLength) { $allLog.Substring($priorLength) } else { '' }
    $required = @(
        'qa-plane-tree-impact-fixture,',
        'plane-tree-impact,',
        'f16-wing-breakaway,',
        'f16-fuselage-ground-contact,',
        'f16-wing-fragment-ground-contact,',
        'qa-plane-tree-impact-confirmed,'
    )
    foreach ($marker in $required) {
        if ($runText -notmatch [regex]::Escape($marker)) { throw "Missing runtime evidence: $marker" }
    }
    $screenshot = Get-ChildItem (Join-Path $root 'screenshots') -Filter '*.png' -File |
        Where-Object { $_.LastWriteTime -ge $start } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (!$screenshot) { throw 'The fixture exited without creating a fresh in-game screenshot.' }
    $summary = [string](@($runText -split "`r?`n" | Where-Object { $_ -match '^summary,' } | Select-Object -Last 1)[0])
    $evidence = @($runText -split "`r?`n" | Where-Object { $_ -match 'qa-plane-tree-impact-fixture|plane-tree-impact,|f16-wing-breakaway,|f16-fuselage-ground-contact,|f16-wing-fragment-ground-contact|qa-plane-tree-impact-confirmed,' } | ForEach-Object { [string]$_ })
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("timestamp=$($start.ToString('yyyy-MM-ddTHH:mm:ssK'))")
    $lines.Add('result=tree-strike-breakup-and-ground-contact')
    $lines.Add("executable_exit_code=$($p.ExitCode)")
    $lines.Add("screenshot=$($screenshot.FullName)")
    foreach ($line in $evidence) { $lines.Add($line) }
    if ($summary) { $lines.Add($summary) }
    [System.IO.File]::AppendAllText($qaLog,($lines.ToArray() -join "`r`n")+"`r`n")
    Write-Output "F-16 breakup QA passed: $($screenshot.Name)"
} finally {
    if ($p -and !$p.HasExited) {
        [void][BwF16BreakupQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
        if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
    }
}

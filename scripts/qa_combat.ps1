$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$qaLog = Join-Path $root 'logs\qa_combat.log'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { [System.IO.File]::ReadAllText($sessionLog).Length } else { 0 }
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Drawing;
using System.Drawing.Imaging;
public static class BwCombatQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int command);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c,uint t);
 public static void Down(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0100,(IntPtr)k,(IntPtr)(1u|(sc<<16)|ex));}
 public static void Up(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0101,(IntPtr)k,(IntPtr)(0xC0000001u|(sc<<16)|ex));}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(140);}
 public static void Hold(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(140);}
 public static void Chord(IntPtr h,int a,int b,int ms){Down(h,a);Down(h,b);Thread.Sleep(ms);Up(h,b);Up(h,a);Thread.Sleep(140);}
 public static void MouseRight(IntPtr h,bool down){PostMessage(h,down?0x0204u:0x0205u,(IntPtr)(down?2:0),(IntPtr)((360<<16)|640));}
 public static void Capture(IntPtr h,string path){RECT r;if(!GetWindowRect(h,out r))throw new Exception("window rect failed");using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(path,ImageFormat.Png);}}
}
"@
$p = Start-Process -FilePath $exe -ArgumentList @('--qa-combat','--qa-silent') -WorkingDirectory $root -PassThru
try {
 $null=$p.WaitForInputIdle(10000)
 $deadline=(Get-Date).AddSeconds(12);$fixtureComplete=$false
 while((Get-Date) -lt $deadline){
  $p.Refresh()
  $current=[System.IO.File]::ReadAllText($sessionLog)
  $runStart=$current.LastIndexOf('mission-start')
  $run=if($runStart -ge 0){$current.Substring($runStart)}else{''}
  $captures=[regex]::Matches($run,'screenshot-taken').Count
  if($run -match 'enemy-down' -and $captures -ge 2){$fixtureComplete=$true;break}
  if($p.HasExited){break}
  Start-Sleep -Milliseconds 200
 }
 if(-not $fixtureComplete){throw 'Combat fixture did not finish its burst, takedown and post-kill renderer capture within 12 seconds.'}
} finally {
 if($p -and !$p.HasExited){[BwCombatQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(!$p.WaitForExit(5000)){Stop-Process -Id $p.Id -Force}}
}
$afterFire=[System.IO.File]::ReadAllText($sessionLog)
$runStart=$afterFire.LastIndexOf('mission-start')
$segment=if($runStart -ge 0){$afterFire.Substring($runStart)}else{''}
$summary=[regex]::Match($segment,'summary,elapsed=[^\r\n]+')
if(!$summary.Success -or $summary.Value -notmatch 'hits=([1-9]\d*)'){
 Add-Content -LiteralPath $qaLog -Value "timestamp=$stamp`nfixture=qa-combat-fixture-target-16m`nresult=failed-to-confirm-hit`nsummary=$($summary.Value)`n" -Encoding UTF8
 throw "Combat input ran but no hit was confirmed. Summary: $($summary.Value)"
}
if($segment -notmatch 'qa-combat-auto-fire'){throw 'The in-game combat auto-fire fixture did not report its test shot.'}
$enemyDown=$segment -match 'enemy-down'
if(-not $enemyDown){throw 'Combat burst registered damage but did not produce an enemy takedown.'}
if($segment -notmatch 'enemy-fire'){throw 'The alert enemy produced no attack response during the combat fixture.'}
$captureCount=[regex]::Matches($segment,'screenshot-taken').Count
if($captureCount -lt 1){throw 'Combat produced no in-game screenshot evidence.'}
$hitCount=[int]([regex]::Match($summary.Value,'hits=(\d+)').Groups[1].Value)
$shotCount=[int]([regex]::Match($summary.Value,'shots=(\d+)').Groups[1].Value)
if($hitCount -lt 4 -or $shotCount -lt 4){throw "Combat burst did not log four hits/shots: shots=$shotCount hits=$hitCount"}
Add-Content -LiteralPath $qaLog -Value "timestamp=$stamp`nfixture=qa-combat-fixture-target-16m`nshots=$shotCount`nhits=$hitCount`nenemy_fire=true`nenemy_down=$enemyDown`nin_game_captures=$captureCount`ninput=deterministic in-game QA fire; normal key controls were separately exercised in run 20261025_001556`n" -Encoding UTF8
Write-Output "Deterministic combat QA passed: $shotCount shots, $hitCount hits, enemy_fire=true, enemy_down=$enemyDown, in_game_captures=$captureCount ($stamp)"

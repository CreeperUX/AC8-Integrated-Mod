$ErrorActionPreference='Stop'
# All filesystem operations below target isolated fixtures, independent of real game processes.
function Get-Process {param($Name,$ErrorAction) return $null}
$repo=Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'package-template/Cleanup-Core.ps1')
$runtime=Get-Content -LiteralPath (Join-Path $repo 'runtime-dependencies.json') -Raw | ConvertFrom-Json
if($AC8LoaderHash -ne $runtime.files[0].sha256){throw 'Historical loader hash differs from pinned runtime'}
$root=Join-Path $repo ('native/test-runtime/cleanup-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$passed=0
function New-Fixture([string]$Name){
 $base=Join-Path $root $Name;$game=Join-Path $base 'game';$w64=Join-Path $game 'Game/Binaries/Win64';$ue=Join-Path $w64 'ue4ss'
 New-Item -ItemType Directory -Path $ue -Force | Out-Null
 Set-Content -LiteralPath (Join-Path $w64 'AceCombat8.exe') -Value 'fixture only'
 $id=[guid]::NewGuid().ToString()
 Set-Content -LiteralPath (Join-Path $ue 'AC8SourceInit-owner.txt') -Value $id
 Set-Content -LiteralPath (Join-Path $ue 'sample.txt') -Value 'runtime data'
 Set-Content -LiteralPath (Join-Path $w64 'dwmapi.dll') -Value 'fixture loader'
 Set-Content -LiteralPath (Join-Path $w64 'steam_appid.txt') -Value '2288340' -Encoding ASCII
 $files=@('dwmapi.dll','steam_appid.txt' | ForEach-Object {[pscustomobject]@{Name=$_;SHA256=(Get-FileHash -LiteralPath (Join-Path $w64 $_)).Hash}})
 return [pscustomobject]@{Game=$game;W64=$w64;UE=$ue;Backup=(Join-Path $base 'backups');State=[pscustomobject]@{Win64=$w64;Id=$id;Files=$files;Results=(Join-Path $base 'missing-results')}}
}
function Assert-Clean($f){foreach($n in 'dwmapi.dll','ue4ss','steam_appid.txt'){if(Test-Path -LiteralPath (Join-Path $f.W64 $n)){throw "Residual $n"}}}
function Expect-Refusal($Action,[string]$Reason){
 $caught=$false;try{& $Action}catch{if($_.Exception.Message -notlike "*$Reason*"){throw};$caught=$true}
 if(!$caught){throw "Expected refusal: $Reason"}
}
function Pass([string]$Name){$script:passed++;Write-Host "PASS $Name"}
$f=New-Fixture 'normal';$a=Invoke-AC8Cleanup $f.Game $f.Backup $f.State
Assert-Clean $f
if(!(Test-Path -LiteralPath (Join-Path $a 'ue4ss/sample.txt'))){throw 'Backup missing'}
if((Get-Content -LiteralPath (Join-Path $a 'cleanup-report.json') -Raw | ConvertFrom-Json).Status -ne 'complete'){throw 'Report incomplete'}
Pass 'normal cleanup, verified backup and completion report'
$null=Invoke-AC8Cleanup $f.Game $f.Backup $f.State;Assert-Clean $f;Pass 'repeat completed cleanup'
$f=New-Fixture 'missing-state';Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup} 'No session record'
if(!(Test-Path -LiteralPath $f.UE)){throw 'Unexpected mutation'};Pass 'missing state reports residual instead of false success'
$f=New-Fixture 'missing-directory';Remove-Item -LiteralPath $f.UE -Recurse -Force
$null=Invoke-AC8Cleanup $f.Game $f.Backup $f.State;Assert-Clean $f;Pass 'DLL-only partial cleanup recovery'
$f=New-Fixture 'missing-marker';Remove-Item -LiteralPath (Join-Path $f.UE 'AC8SourceInit-owner.txt')
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'marker missing';Pass 'unowned directory retained'
$f=New-Fixture 'mismatched-marker';Set-Content -LiteralPath (Join-Path $f.UE 'AC8SourceInit-owner.txt') -Value 'other'
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'mismatched';Pass 'mismatched ownership retained'
$f=New-Fixture 'tamper';Add-Content -LiteralPath (Join-Path $f.W64 'dwmapi.dll') -Value 'tamper'
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'file changed'
if(!(Test-Path -LiteralPath $f.UE)){throw 'Mutation before validation'};Pass 'changed DLL blocks all mutation'
$f=New-Fixture 'unrecorded';$f.State.Files=@()
# Substitute only the fixture loader hash; pin equality was verified above.
$originalHash=$AC8LoaderHash;$AC8LoaderHash=(Get-FileHash -LiteralPath (Join-Path $f.W64 'dwmapi.dll')).Hash
$null=Invoke-AC8Cleanup $f.Game $f.Backup $f.State;Assert-Clean $f;Pass 'unrecorded recognized DLL and AppID recovered'
$f=New-Fixture 'historical';$null=Invoke-AC8Cleanup $f.Game $f.Backup -RecoverHistorical;Assert-Clean $f;Pass 'historical recovery without package state'
$f=New-Fixture 'unknown-dll';$AC8LoaderHash=$originalHash
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup -RecoverHistorical} 'Unknown dwmapi';Pass 'unknown historical loader retained'
$f=New-Fixture 'other-mod';New-Item -ItemType Directory -Path (Join-Path $f.UE 'Mods/AnotherMod') -Force | Out-Null
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup -RecoverHistorical} 'Other UE4SS mods';Pass 'historical directory with other mods retained'
$f=New-Fixture 'check';$null=Invoke-AC8Cleanup $f.Game $f.Backup $f.State -CheckOnly
if(!(Test-Path -LiteralPath $f.UE) -or (Test-Path -LiteralPath $f.Backup)){throw 'CheckOnly mutated files'};Pass 'read-only check'
$f=New-Fixture 'inside-backup';Expect-Refusal {Invoke-AC8Cleanup $f.Game (Join-Path $f.Game 'backup') $f.State} 'outside the game';Pass 'backup path boundary'
$f=New-Fixture 'wrong-game';$f.State.Win64=$root
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'path changed';Pass 'state path mismatch'
$f=New-Fixture 'running'
& {function Get-Process {param($Name,$ErrorAction) return [pscustomobject]@{Id=123}};Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'Close AC8'}
Pass 'running game refused'
$f=New-Fixture 'backup-failure'
& {function Copy-Item {throw 'TEST_BACKUP_FAILURE'};Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'TEST_BACKUP_FAILURE'}
foreach($n in 'dwmapi.dll','ue4ss','steam_appid.txt'){if(!(Test-Path -LiteralPath (Join-Path $f.W64 $n))){throw 'Backup failure removed files'}}
Pass 'backup failure leaves deployed files intact'
$f=New-Fixture 'interrupted-removal'
& {
 function Remove-Item {param($LiteralPath,[switch]$Recurse,[switch]$Force)
  if([IO.Path]::GetFileName($LiteralPath) -eq 'steam_appid.txt'){throw 'TEST_REMOVAL_FAILURE'}
  Microsoft.PowerShell.Management\Remove-Item -LiteralPath $LiteralPath -Recurse:$Recurse -Force:$Force
 }
 Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'TEST_REMOVAL_FAILURE'
}
$null=Invoke-AC8Cleanup $f.Game $f.Backup $f.State;Assert-Clean $f;Pass 'retry after DLL and directory already removed'
$f=New-Fixture 'missing-saves';$package=Join-Path (Split-Path $f.Game -Parent) 'package'
New-Item -ItemType Directory -Path $package | Out-Null
foreach($n in 'PowerShell-Compat.ps1','Launch-Offline.ps1','Cleanup-Core.ps1','Install-Common.ps1'){Copy-Item -LiteralPath (Join-Path $repo ('package-template/'+$n)) -Destination $package}
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value $f.Game -Encoding UTF8
$f.State | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'active-session.json') -Encoding UTF8
$before=$env:LOCALAPPDATA
try{$env:LOCALAPPDATA=Join-Path $root 'nonexistent-saves'; & (Join-Path $package 'Launch-Offline.ps1') -CleanupOnly}finally{$env:LOCALAPPDATA=$before}
Assert-Clean $f
if(Test-Path -LiteralPath (Join-Path $package 'active-session.json')){throw 'Missing saves kept active state'}
Pass 'actual launcher cleanup retires state before missing-save diagnostics'
if(Test-Path -LiteralPath $f.State.Results){throw 'Invalid diagnostics destination was created'}
Pass 'invalid session diagnostics path is not written'
# On Windows a junction requires no developer mode or symlink privilege.
$f=New-Fixture 'junction';$outside=Join-Path $root 'outside';New-Item -ItemType Directory -Path $outside | Out-Null
Set-Content -LiteralPath (Join-Path $outside 'keep.txt') -Value 'keep'
New-Item -ItemType Junction -Path (Join-Path $f.UE 'linked') -Target $outside | Out-Null
Expect-Refusal {Invoke-AC8Cleanup $f.Game $f.Backup $f.State} 'Filesystem link'
if(!(Test-Path -LiteralPath (Join-Path $outside 'keep.txt'))){throw 'Junction target changed'}
Pass 'junction refused without traversing or changing target'
$f=New-Fixture 'deployment-interruption'
foreach($n in 'dwmapi.dll','ue4ss','steam_appid.txt'){Remove-Item -LiteralPath (Join-Path $f.W64 $n) -Recurse -Force}
$package=Join-Path (Split-Path $f.Game -Parent) 'package'
Copy-Item -LiteralPath (Join-Path $repo 'package-template') -Destination $package -Recurse
foreach($n in 'tools','models'){Copy-Item -LiteralPath (Join-Path $repo $n) -Destination $package -Recurse}
$payload=Join-Path $package 'payload'
Set-Content -LiteralPath (Join-Path $payload 'Game/Binaries/Win64/dwmapi.dll') -Value 'fixture loader'
Set-Content -LiteralPath (Join-Path $package 'package-info.json') -Value '{}'
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value $f.Game -Encoding UTF8
$manifest=@(Get-ChildItem -LiteralPath $payload -File -Recurse | ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($payload.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
ConvertTo-Json -InputObject $manifest -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'payload-manifest.json') -Encoding UTF8
$before=$env:LOCALAPPDATA
try {
 $env:LOCALAPPDATA=Join-Path $root 'mock-saves'
 New-Item -ItemType Directory -Path (Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames') -Force | Out-Null
 & {
  function Get-Process {param($Name,$ErrorAction) if($Name -eq 'steam'){return [pscustomobject]@{Id=123}}}
  function Get-AC8FileHash {param($LiteralPath)
   if([IO.Path]::GetFileName($LiteralPath) -eq 'AceCombat8.exe'){return [pscustomobject]@{Hash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'}}
   Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
  }
  function Copy-Item {param($LiteralPath,$Destination,[switch]$Recurse,[switch]$Force)
   Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination -Recurse:$Recurse -Force:$Force
   if([IO.Path]::GetFileName($LiteralPath) -eq 'dwmapi.dll' -and $Destination -eq $f.W64){
    $s=Get-Content -LiteralPath (Join-Path $package 'active-session.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($s.Files | Where-Object Name -eq 'dwmapi.dll').Count -ne 1){throw 'Deployment intent was not recorded'}
    throw 'TEST_AFTER_DLL_COPY'
   }
  }
  function Start-Process {throw 'Tests must never start a game process'}
  Expect-Refusal {& (Join-Path $package 'Launch-Offline.ps1')} 'TEST_AFTER_DLL_COPY'
 }
}finally{$env:LOCALAPPDATA=$before}
Assert-Clean $f
if(Test-Path -LiteralPath (Join-Path $package 'active-session.json')){throw 'Interrupted deployment kept active state'}
Pass 'actual launcher interruption after DLL copy cleans recorded intent without starting game'
Write-Host "PASS $passed cleanup regressions. Fixtures: $root"

param([switch]$CleanupOnly)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$gameRoot=([string](Get-Content -LiteralPath (Join-Path $root 'game-path.txt') -Raw -Encoding UTF8)).Trim()
if(!$gameRoot){throw 'Run Setup.cmd first.'}
$w64=[IO.Path]::GetFullPath((Join-Path $gameRoot 'Game/Binaries/Win64'))
$exe=Join-Path $w64 'AceCombat8.exe'
$statePath=Join-Path $root 'active-session.json'
$expectedHash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'
function Get-SaveHashes {
 $save=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
 @(Get-ChildItem -LiteralPath $save -File | ForEach-Object {
  [pscustomobject]@{Name=$_.Name;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
 })
}
function Cleanup-Owned($state) {
 if(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue){throw 'Close the game normally before cleanup.'}
 if($state.Win64 -ne $w64){throw 'Session game path changed; refusing cleanup.'}
 $ownedRoot=Join-Path $w64 'ue4ss'
 $marker=Join-Path $ownedRoot 'AC8SourceInit-owner.txt'
 if(!(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne $state.Id){throw 'Ownership marker missing or mismatched.'}
 $links=@(Get-Item -LiteralPath $ownedRoot; Get-ChildItem -LiteralPath $ownedRoot -Recurse -Force) | Where-Object {($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0}
 if($links){throw 'Unexpected filesystem link in staged directory. Leaving files for review.'}
 # Preserve all contents before removal, including anything added during this run.
 Copy-Item -LiteralPath $ownedRoot -Destination (Join-Path $state.Results 'runtime-copy') -Recurse -Force
 $log=Join-Path $ownedRoot 'UE4SS.log'
 if(Test-Path -LiteralPath $log){Copy-Item -LiteralPath $log -Destination (Join-Path $state.Results 'UE4SS.log')}
 $approvedFiles=@()
 foreach($file in $state.Files){
  $target=[IO.Path]::GetFullPath((Join-Path $w64 $file.Name))
  if($target -notin @((Join-Path $w64 'dwmapi.dll'),(Join-Path $w64 'steam_appid.txt'))){throw 'Unexpected owned file path.'}
  if(Test-Path -LiteralPath $target){
   if((Get-FileHash -LiteralPath $target).Hash -ne $file.SHA256){throw 'A staged file changed; preserved for review.'}
   $approvedFiles+=$target
  }
 }
 foreach($target in $approvedFiles){Remove-Item -LiteralPath $target -Force}
 $resolved=(Resolve-Path -LiteralPath $ownedRoot).Path
 if($resolved -ne (Join-Path $w64 'ue4ss')){throw 'Unexpected recursive cleanup path.'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
 $after=Get-SaveHashes
 ConvertTo-Json -InputObject $after | Set-Content -LiteralPath (Join-Path $state.Results 'save-hashes-after.json') -Encoding UTF8
 Move-Item -LiteralPath $statePath -Destination (Join-Path $state.Results 'completed-session.json')
 Write-Host "Finished. Session log: $($state.Results)\UE4SS.log"
 # Analysis happens after owned loader cleanup. Failure never restores/replaces saves.
 try {
  if(Get-Command python.exe -ErrorAction SilentlyContinue){
   & python.exe -X utf8 (Join-Path $root 'tools/analyze_experiment.py') --session $state.Results
   if($LASTEXITCODE -ne 0){Write-Host 'Shadow analysis failed; raw CSV retained. Use Analyze-Latest.cmd later.'}
  }else{Write-Host 'Python unavailable; raw CSV retained. Analysis needs Python and NumPy.'}
 }catch{Write-Host ('Analysis skipped; raw data retained: '+$_.Exception.Message)}

}
if($CleanupOnly){
 if(!(Test-Path -LiteralPath $statePath)){Write-Host 'No active candidate session.';exit 0}
 Cleanup-Owned (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json)
 exit 0
}
$validation=Get-Content -LiteralPath (Join-Path $root 'validation-status.json') -Raw | ConvertFrom-Json
if($validation.deploymentAllowed -ne $true){throw 'Candidate is gated off. No game files staged. See validation-status.json.'}
if(Test-Path -LiteralPath $statePath){throw 'Previous candidate session needs cleanup. Close game and run Cleanup-Offline.cmd.'}
if(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
if(!(Get-Process -Name steam -ErrorAction SilentlyContinue)){throw 'Start Steam first.'}
if(!(Test-Path -LiteralPath $exe) -or (Get-FileHash -LiteralPath $exe).Hash -ne $expectedHash){throw 'Game build mismatch. This candidate supports build 25201480 only.'}
foreach($n in 'dwmapi.dll','ue4ss','steam_appid.txt'){
 if(Test-Path -LiteralPath (Join-Path $w64 $n)){throw "Existing loader conflict: $n. Nothing overwritten."}
}
$payloadRoot=[IO.Path]::GetFullPath((Join-Path $root 'payload'))
$manifest=Get-Content -LiteralPath (Join-Path $root 'payload-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$actualFiles=@(Get-ChildItem -LiteralPath $payloadRoot -File -Recurse)
if($actualFiles.Count -ne $manifest.Count){throw 'Payload file count differs from manifest.'}
foreach($entry in $manifest){
 $file=[IO.Path]::GetFullPath((Join-Path $payloadRoot $entry.Path))
 if(!$file.StartsWith($payloadRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload path escaped package.'}
 if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ne $entry.SHA256){throw 'Payload hash verification failed.'}
}
$id=[guid]::NewGuid().ToString()
$results=Join-Path $root ('sessions/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$id.Substring(0,8))
New-Item -ItemType Directory -Path $results -Force | Out-Null
$saveRoot=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
$saveBackup=Join-Path $results 'SaveGames-before'
New-Item -ItemType Directory -Path $saveBackup -Force | Out-Null
Get-ChildItem -LiteralPath $saveRoot -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $saveBackup}
$before=Get-SaveHashes
ConvertTo-Json -InputObject $before | Set-Content -LiteralPath (Join-Path $results 'save-hashes-before.json') -Encoding UTF8
foreach($saveFile in $before){if((Get-FileHash -LiteralPath (Join-Path $saveBackup $saveFile.Name)).Hash -ne $saveFile.SHA256){throw 'Save backup verification failed.'}}
. (Join-Path $root 'MouseAim-Settings.ps1')
$mouseSettings=Read-MouseSettings (Join-Path $root 'MouseAim-Settings.ini')
. (Join-Path $root 'Feature-Settings.ps1')
$features=Read-FeatureSettings (Join-Path $root 'features.ini')
$readinessPattern=Get-FeatureReadinessPattern $features
$features | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $results 'feature-selection.json') -Encoding UTF8
$payload=Join-Path $root 'payload/Game/Binaries/Win64'
$state=[pscustomobject]@{Id=$id;Win64=$w64;Results=$results;Files=@();PID=$null}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
try {
 New-Item -ItemType Directory -Path (Join-Path $w64 'ue4ss') | Out-Null
 Set-Content -LiteralPath (Join-Path $w64 'ue4ss/AC8SourceInit-owner.txt') -Value $id -Encoding ASCII
 Install-SelectedUE4SS (Join-Path $payload 'ue4ss') (Join-Path $w64 'ue4ss') $features
 Apply-MouseSettings $mouseSettings (Join-Path $w64 'ue4ss/Mods/AC8MouseAim')
 Copy-Item -LiteralPath (Join-Path $payload 'dwmapi.dll') -Destination $w64
 $state.Files+= [pscustomobject]@{Name='dwmapi.dll';SHA256=(Get-FileHash -LiteralPath (Join-Path $w64 'dwmapi.dll')).Hash}
 $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
 Set-Content -LiteralPath (Join-Path $w64 'steam_appid.txt') -Value '2288340' -Encoding ASCII
 $state.Files+= [pscustomobject]@{Name='steam_appid.txt';SHA256=(Get-FileHash -LiteralPath (Join-Path $w64 'steam_appid.txt')).Hash}
 $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
 $env:SteamAppId='2288340';$env:SteamGameId='2288340';$env:EOS_USE_ANTICHEATCLIENTNULL='1'
 Write-Host 'AC8 2.3.0 OPTIONAL MISSILE MODULE + F4 CLASSIC/AGILE (world direction target, paired input, arrival braking) - gameplay acceptance incomplete. Single-player only. Keep this console open.'
 Write-Host 'Mouse Aim: select Expert controls. F8 instructor; F9 recenter; F10 reload mouse settings; hold C for free look.'
 if($features.MissileEnhancement){Write-Host 'FEATURES: mouse flight + missile enhancement/cosmetics.'}else{Write-Host 'FEATURES: mouse flight only. Missile module is not installed.'}
 $game=Start-Process -FilePath $exe -WorkingDirectory $w64 -WindowStyle Normal -PassThru
 $state.PID=$game.Id
 $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
 # Observe this process and its log only; no game memory/object polling.
 $ready=$false
 $deadline=[DateTime]::UtcNow.AddSeconds(45)
 $runtimeLog=Join-Path $w64 'ue4ss/UE4SS.log'
 while(!$game.HasExited -and [DateTime]::UtcNow -lt $deadline){
  if(Test-Path -LiteralPath $runtimeLog){
   try {$ready=[bool](Select-String -LiteralPath $runtimeLog -Pattern $readinessPattern -Quiet -ErrorAction Stop)} catch {$ready=$false}
  }
  if($ready){break}
  [void]$game.WaitForExit(500)
 }
 if($ready){
  if($features.MissileEnhancement){Write-Host '[SOURCE READY] 375 source fields verified; actual missile behavior depends on game mechanics.'}else{Write-Host '[MOUSE READY] Mouse module loaded; original missile behavior preserved.'}
  Write-Host 'This is a candidate, not the completed release. Event recording is automatic.'
 }elseif(!$game.HasExited){
  Write-Host '[NOT READY] The selected feature set has not reported readiness. Check the archived log.'
 }else{
  Write-Host '[EXITED] Game ended before source initialization readiness. Log will be preserved.'
 }
 $game.WaitForExit()
} finally {
 if(!(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue)){
  Cleanup-Owned (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json)
 }else{Write-Host 'Game still active; staged files retained. Close normally, then run Cleanup-Offline.cmd.'}
}

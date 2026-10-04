param([switch]$CleanupOnly,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
if($CleanupOnly -and $CheckOnly){throw 'CleanupOnly and CheckOnly cannot be combined.'}
$root=$PSScriptRoot
. (Join-Path $root 'Install-Common.ps1')
if(!$CleanupOnly){& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root}
$gameRoot=([string](Get-Content -LiteralPath (Join-Path $root 'game-path.txt') -Raw -Encoding UTF8)).Trim()
$gameRoot=Resolve-AC8GameRoot $gameRoot
Assert-AC8PackageLocation $root $gameRoot
$w64=[IO.Path]::GetFullPath((Join-Path $gameRoot 'Game/Binaries/Win64'))
$exe=Join-Path $w64 'AceCombat8.exe'
$statePath=Join-Path $root 'active-session.json'
. (Join-Path $root 'Cleanup-Core.ps1')
Assert-AC8NoLinks $w64
$expectedHash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'
$operation=$null
if(!$CheckOnly){$operation=Enter-AC8Operation $gameRoot}
try {
function Get-SaveHashes {
 $save=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
 if(!(Test-Path -LiteralPath $save)){return @()}
 @(Get-ChildItem -LiteralPath $save -File | ForEach-Object {
  [pscustomobject]@{Name=$_.Name;SHA256=(Get-AC8FileHash -LiteralPath $_.FullName).Hash}
 })
}
function Cleanup-Owned($state,[bool]$Analyze=$true) {
 $archive=Invoke-AC8Cleanup -GameRoot $gameRoot -BackupRoot (Join-Path $root 'cleanup-backups') -State $state
 if(!$archive){$archive=Join-Path $root ('cleanup-backups/completed-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $archive -Force | Out-Null}
 Move-Item -LiteralPath $statePath -Destination (Join-Path $archive 'completed-session.json')
 # Cleanup is complete before optional diagnostics. Missing saves cannot block recovery.
 try {
  $resultsPath=[IO.Path]::GetFullPath([string]$state.Results)
  $sessionsPath=[IO.Path]::GetFullPath((Join-Path $root 'sessions'))+[IO.Path]::DirectorySeparatorChar
  if(!$resultsPath.StartsWith($sessionsPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Session results path escaped this package; diagnostics skipped.'}
  New-Item -ItemType Directory -Path $state.Results -Force | Out-Null
  $runtime=Join-Path $archive 'ue4ss'
  if(Test-Path -LiteralPath $runtime){
   Copy-Item -LiteralPath $runtime -Destination (Join-Path $state.Results 'runtime-copy') -Recurse -Force
   $log=Join-Path $runtime 'UE4SS.log'
   if(Test-Path -LiteralPath $log){Copy-Item -LiteralPath $log -Destination (Join-Path $state.Results 'UE4SS.log') -Force}
  }
  $after=Get-SaveHashes
  ConvertTo-Json -InputObject $after | Set-Content -LiteralPath (Join-Path $state.Results 'save-hashes-after.json') -Encoding UTF8
  if($Analyze){Invoke-AC8OptionalAnalysis $root $state.Results}
 }catch{Write-Host ('清理已完成，可选诊断未完成：'+$_.Exception.Message)}
}

if($CleanupOnly){
 if(!(Test-Path -LiteralPath $statePath)){$null=Invoke-AC8Cleanup -GameRoot $gameRoot -BackupRoot (Join-Path $root 'cleanup-backups');exit 0}
 Cleanup-Owned (Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json)
 exit 0
}
$validation=Get-Content -LiteralPath (Join-Path $root 'validation-status.json') -Raw | ConvertFrom-Json
if($validation.deploymentAllowed -ne $true){throw 'Candidate is gated off. No game files staged. See validation-status.json.'}
if(Test-Path -LiteralPath $statePath){Stop-AC8Problem 'OLD_SESSION' '上一次运行尚未完成清理。' '正常关闭游戏后运行 Cleanup-Offline.cmd，再从 Steam 或 Start.cmd 启动。'}
if(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
if(!(Get-Process -Name steam -ErrorAction SilentlyContinue)){throw 'Start Steam first.'}
if(!(Test-Path -LiteralPath $exe) -or (Get-AC8FileHash -LiteralPath $exe).Hash -ne $expectedHash){throw 'Game build mismatch. This candidate supports build 25201480 only.'}
foreach($n in 'dwmapi.dll','ue4ss','steam_appid.txt'){
 if(Test-Path -LiteralPath (Join-Path $w64 $n)){Stop-AC8Problem 'LOADER_CONFLICT' "检测到已有加载器：$n" '请从 Steam 或 Start.cmd 进入恢复流程，或运行 Recover-Cleanup.cmd 核对归属后清理。'}
}
$payloadRoot=[IO.Path]::GetFullPath((Join-Path $root 'payload'))
$manifest=Get-Content -LiteralPath (Join-Path $root 'payload-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$actualFiles=@(Get-ChildItem -LiteralPath $payloadRoot -File -Recurse)
if($actualFiles.Count -ne $manifest.Count){throw 'Payload file count differs from manifest.'}
foreach($entry in $manifest){
 $file=[IO.Path]::GetFullPath((Join-Path $payloadRoot $entry.Path))
 if(!$file.StartsWith($payloadRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload path escaped package.'}
 if(!(Test-Path -LiteralPath $file) -or (Get-AC8FileHash -LiteralPath $file).Hash -ne $entry.SHA256){throw 'Payload hash verification failed.'}
}
if($CheckOnly){Write-Host 'LAUNCH PREFLIGHT passed. No files staged and no game started.';exit 0}
Assert-AC8WriteAccess $root
Assert-AC8WriteAccess $w64
$id=[guid]::NewGuid().ToString()
$results=Join-Path $root ('sessions/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$id.Substring(0,8))
New-Item -ItemType Directory -Path $results -Force | Out-Null
$saveRoot=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
$saveBackup=Join-Path $results 'SaveGames-before'
New-Item -ItemType Directory -Path $saveBackup -Force | Out-Null
if(Test-Path -LiteralPath $saveRoot){Get-ChildItem -LiteralPath $saveRoot -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $saveBackup}}
$before=Get-SaveHashes
ConvertTo-Json -InputObject $before | Set-Content -LiteralPath (Join-Path $results 'save-hashes-before.json') -Encoding UTF8
foreach($saveFile in $before){if((Get-AC8FileHash -LiteralPath (Join-Path $saveBackup $saveFile.Name)).Hash -ne $saveFile.SHA256){throw 'Save backup verification failed.'}}
. (Join-Path $root 'MouseAim-Settings.ps1')
$mouseSettings=Read-MouseSettings (Join-Path $root 'MouseAim-Settings.ini')
. (Join-Path $root 'Feature-Settings.ps1')
$features=Read-FeatureSettings (Join-Path $root 'features.ini')
$readinessPattern=Get-FeatureReadinessPattern $features
$features | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $results 'feature-selection.json') -Encoding UTF8
$payload=Join-Path $root 'payload/Game/Binaries/Win64'
# Record intended deployment before copying files, so interruption cannot orphan a DLL.
$sha=[Security.Cryptography.SHA256]::Create()
try{$appidHash=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::ASCII.GetBytes("2288340`r`n"))).Replace('-','')}finally{$sha.Dispose()}
$planned=@([pscustomobject]@{Name='dwmapi.dll';SHA256=(Get-AC8FileHash -LiteralPath (Join-Path $payload 'dwmapi.dll')).Hash},[pscustomobject]@{Name='steam_appid.txt';SHA256=$appidHash})
$state=[pscustomobject]@{Id=$id;Win64=$w64;Results=$results;Files=$planned;PID=$null}
Write-AC8Json $state $statePath
$launchFailure=$null
$stage='准备加载器'
try {
 New-Item -ItemType Directory -Path (Join-Path $w64 'ue4ss') | Out-Null
 Set-Content -LiteralPath (Join-Path $w64 'ue4ss/AC8SourceInit-owner.txt') -Value $id -Encoding ASCII
 Install-SelectedUE4SS (Join-Path $payload 'ue4ss') (Join-Path $w64 'ue4ss') $features
 Apply-MouseSettings $mouseSettings (Join-Path $w64 'ue4ss/Mods/AC8MouseAim')
 $stage="写入加载器 $(Join-Path $w64 'dwmapi.dll')"
 Copy-Item -LiteralPath (Join-Path $payload 'dwmapi.dll') -Destination $w64
 Write-AC8Json $state $statePath
 Set-Content -LiteralPath (Join-Path $w64 'steam_appid.txt') -Value '2288340' -Encoding ASCII
 Write-AC8Json $state $statePath
 $env:SteamAppId='2288340';$env:SteamGameId='2288340';$env:EOS_USE_ANTICHEATCLIENTNULL='1'
 Write-Host 'AC8 2.3.4 OPTIONAL MISSILE MODULE + F4 CLASSIC/AGILE (world direction target, paired input, arrival braking) - gameplay acceptance incomplete. Single-player only. Keep this console open.'
 Write-Host 'Mouse Aim: select Expert controls. F8 instructor; F9 recenter; F10 reload mouse settings; hold C for free look.'
 Write-Host ('FEATURES: mouse flight; missile mode='+$features.MissileMode)
 $stage='启动游戏'
 $game=Start-Process -FilePath $exe -WorkingDirectory $w64 -WindowStyle Normal -PassThru
 $state.PID=$game.Id
 Write-AC8Json $state $statePath
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
  if($features.MissileEnhancement){Write-Host ('[SOURCE READY] mode='+$features.MissileMode+' verified='+$features.ExpectedFields+'; actual missile behavior depends on game mechanics.')}else{Write-Host '[MOUSE READY] Mouse module loaded; original missile behavior preserved.'}
  Write-Host 'This is a candidate, not the completed release. Event recording is automatic.'
 }elseif(!$game.HasExited){
  Write-Host '[NOT READY] The selected feature set has not reported readiness. Check the archived log.'
 }else{
  Write-Host '[EXITED] Game ended before source initialization readiness. Log will be preserved.'
 }
 $game.WaitForExit()
 }catch{
 $launchFailure=$_
 Show-AC8Problem $launchFailure $root $stage
 throw
} finally {
 if(!(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue)){
  try{
   Cleanup-Owned (Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json) -Analyze:(!$launchFailure)
  }catch{
   if($launchFailure){Show-AC8Problem $_ $root '后续清理'}else{throw}
  }
 }else{Write-Host '游戏仍在运行，已保留加载器。正常退出后运行 Cleanup-Offline.cmd。'}
}

}finally{Exit-AC8Operation $operation}

param([switch]$CleanupOnly,[switch]$CheckOnly,[switch]$AllowNoSteam,[string]$GameRoot='')
$ErrorActionPreference='Stop'
if($CleanupOnly -and $CheckOnly){throw 'CleanupOnly and CheckOnly cannot be combined.'}
$root=$PSScriptRoot
. (Join-Path $root 'Install-Common.ps1')
# Package files come from $root; user data (settings, sessions, backups) from $data, the same folder when portable.
$data=Get-AC8DataRoot $root
function Assert-ModEnabled {
if(!$CleanupOnly -and !$CheckOnly){
 $disabled=Test-Path -LiteralPath (Join-Path $data 'mod-disabled.flag')
 $parent=Split-Path $PSScriptRoot -Parent
 if((Test-Path -LiteralPath (Join-Path $parent 'AC8-Managed-Packages.json')) -and (Test-Path -LiteralPath (Join-Path $parent 'mod-disabled.flag'))){$disabled=$true}
 if($disabled){throw 'Mod disabled. Use Enable-Mod.cmd to restore offline mod mode.'}
}
}
Assert-ModEnabled
if(!$CleanupOnly){& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root}
if($GameRoot -and $GameRoot.Trim()){
 $gameRoot=Resolve-AC8GameRoot $GameRoot
}else{
 $gameRoot=([string](Get-Content -LiteralPath (Join-Path $data 'game-path.txt') -Raw -Encoding UTF8)).Trim()
 $gameRoot=Resolve-AC8GameRoot $gameRoot
}
Assert-AC8PackageLocation $root $gameRoot
$w64=[IO.Path]::GetFullPath((Join-Path $gameRoot 'Game/Binaries/Win64'))
$exe=Join-Path $w64 'AceCombat8.exe'
$statePath=Join-Path $data 'active-session.json'
. (Join-Path $root 'Cleanup-Core.ps1')
Assert-AC8NoLinks $w64
$expectedHash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'
$operation=$null
if(!$CheckOnly){$operation=Enter-AC8Operation $gameRoot}
try {
Assert-ModEnabled
function Get-SaveHashes {
 $save=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
 if(!(Test-Path -LiteralPath $save)){return @()}
 @(Get-ChildItem -LiteralPath $save -File | ForEach-Object {
  [pscustomobject]@{Name=$_.Name;SHA256=(Get-AC8FileHash -LiteralPath $_.FullName).Hash}
 })
}
function Cleanup-Owned($state,[bool]$Analyze=$true) {
 $archive=Invoke-AC8Cleanup -GameRoot $gameRoot -BackupRoot (Join-Path $data 'cleanup-backups') -State $state
 if(!$archive){$archive=Join-Path $data ('cleanup-backups/completed-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $archive -Force | Out-Null}
 Move-Item -LiteralPath $statePath -Destination (Join-Path $archive 'completed-session.json')
 # Cleanup is complete before optional diagnostics. Missing saves cannot block recovery.
 try {
  $resultsPath=[IO.Path]::GetFullPath([string]$state.Results)
  $sessionsPath=[IO.Path]::GetFullPath((Join-Path $data 'sessions'))+[IO.Path]::DirectorySeparatorChar
  if(!$resultsPath.StartsWith($sessionsPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Session results path escaped this package; diagnostics skipped.'}
  New-Item -ItemType Directory -Path $state.Results -Force | Out-Null
  $runtime=Join-Path $archive 'ue4ss'
  if(Test-Path -LiteralPath $runtime){
   # 2.4.3: the loader and mod DLLs are not copied into every session (about 28 MB per launch); their hashes are kept.
   $base=(Get-Item -LiteralPath $runtime).FullName.TrimEnd('\','/');$copy=Join-Path $state.Results 'runtime-copy';$dlls=@()
   foreach($file in Get-ChildItem -LiteralPath $base -Recurse -File){
    $rel=$file.FullName.Substring($base.Length).TrimStart('\','/')
    if($file.Extension -ieq '.dll'){$dlls+=[pscustomobject]@{Path=$rel;SHA256=(Get-AC8FileHash -LiteralPath $file.FullName).Hash;Bytes=$file.Length};continue}
    $target=Join-Path $copy $rel;New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
   }
   ConvertTo-Json -InputObject $dlls | Set-Content -LiteralPath (Join-Path $state.Results 'runtime-dll-hashes.json') -Encoding UTF8
   $log=Join-Path $runtime 'UE4SS.log'
   if(Test-Path -LiteralPath $log){Copy-Item -LiteralPath $log -Destination (Join-Path $state.Results 'UE4SS.log') -Force}
  }
  $after=Get-SaveHashes
  ConvertTo-Json -InputObject $after | Set-Content -LiteralPath (Join-Path $state.Results 'save-hashes-after.json') -Encoding UTF8
  if($Analyze){Invoke-AC8OptionalAnalysis $root $state.Results}
 }catch{Write-Host ('清理已完成，可选诊断未完成：'+$_.Exception.Message)}
}

if($CleanupOnly){
 if(!(Test-Path -LiteralPath $statePath)){$null=Invoke-AC8Cleanup -GameRoot $gameRoot -BackupRoot (Join-Path $data 'cleanup-backups');exit 0}
 Cleanup-Owned (Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json)
 exit 0
}
$validation=Get-Content -LiteralPath (Join-Path $root 'validation-status.json') -Raw | ConvertFrom-Json
if($validation.deploymentAllowed -ne $true){throw 'Candidate is gated off. No game files staged. See validation-status.json.'}
if(Test-Path -LiteralPath $statePath){Stop-AC8Problem 'OLD_SESSION' '上一次运行尚未完成清理。' '正常关闭游戏后运行 Cleanup-Offline.cmd，再从 Steam 或 Start.cmd 启动。'}
if(Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
if(!$AllowNoSteam -and !(Get-Process -Name steam -ErrorAction SilentlyContinue)){throw 'Start Steam first.'}
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
Assert-AC8WriteAccess $data
Assert-AC8WriteAccess $w64
$id=[guid]::NewGuid().ToString()
$results=Join-Path $data ('sessions/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+$id.Substring(0,8))
New-Item -ItemType Directory -Path $results -Force | Out-Null
$saveRoot=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment/ACE COMBAT 8/Saved/SaveGames'
$saveBackup=Join-Path $results 'SaveGames-before'
New-Item -ItemType Directory -Path $saveBackup -Force | Out-Null
if(Test-Path -LiteralPath $saveRoot){Get-ChildItem -LiteralPath $saveRoot -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $saveBackup}}
$before=Get-SaveHashes
ConvertTo-Json -InputObject $before | Set-Content -LiteralPath (Join-Path $results 'save-hashes-before.json') -Encoding UTF8
foreach($saveFile in $before){if((Get-AC8FileHash -LiteralPath (Join-Path $saveBackup $saveFile.Name)).Hash -ne $saveFile.SHA256){throw 'Save backup verification failed.'}}
. (Join-Path $root 'MouseAim-Settings.ps1')
foreach($migration in @(Invoke-AC8SettingsMigrations $data)){if($migration -eq 'war-default-2.5.0'){Write-Host 'SETTINGS: startup flight control is now WAR (default since 2.5). F4 switches to PEACE.'}}
$mouseSettings=Read-MouseSettings (Join-Path $data 'MouseAim-Settings.ini')
. (Join-Path $root 'Feature-Settings.ps1')
$features=Read-FeatureSettings (Join-Path $data 'features.ini')
$readinessPattern=Get-FeatureReadinessPattern $features
$features | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $results 'feature-selection.json') -Encoding UTF8
$payload=Join-Path $root 'payload/Game/Binaries/Win64'
# Record intended deployment before copying files, so interruption cannot orphan a DLL.
$sha=[Security.Cryptography.SHA256]::Create()
try{$appidHash=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::ASCII.GetBytes("2288340`r`n"))).Replace('-','')}finally{$sha.Dispose()}
$planned=@([pscustomobject]@{Name='dwmapi.dll';SHA256=(Get-AC8FileHash -LiteralPath (Join-Path $payload 'dwmapi.dll')).Hash},[pscustomobject]@{Name='steam_appid.txt';SHA256=$appidHash})
$state=[pscustomobject]@{Id=$id;Win64=$w64;Results=$results;Files=$planned;PID=$null;ExtraMods=@()}
Write-AC8Json $state $statePath
$launchFailure=$null
$stage='准备加载器'
try {
 New-Item -ItemType Directory -Path (Join-Path $w64 'ue4ss') | Out-Null
 Set-Content -LiteralPath (Join-Path $w64 'ue4ss/AC8SourceInit-owner.txt') -Value $id -Encoding ASCII
 $extraMods=@(Get-AC8ExtraMods $data)
 Install-SelectedUE4SS (Join-Path $payload 'ue4ss') (Join-Path $w64 'ue4ss') $features $extraMods
 # Cleanup removes deployed extra-mod copies that still match ue4ss-mods instead of archiving them (2.5.2).
 $state.ExtraMods=@($extraMods | Where-Object {!$_.Problem} | ForEach-Object {[pscustomobject]@{Name=$_.Name;Source=$_.Path}})
 Write-AC8Json $state $statePath
 Apply-MouseSettings $mouseSettings (Join-Path $w64 'ue4ss/Mods/AC8MouseAim')
 . (Join-Path $root 'Keybinding-Settings.ps1')
 $bindings=Read-AC8Keybindings (Join-Path $data 'Keybindings.ini')
 Apply-AC8Keybindings $bindings (Join-Path $w64 'ue4ss/Mods/AC8MouseAim')
 $stage="写入加载器 $(Join-Path $w64 'dwmapi.dll')"
 Copy-Item -LiteralPath (Join-Path $payload 'dwmapi.dll') -Destination $w64
 Write-AC8Json $state $statePath
 Set-Content -LiteralPath (Join-Path $w64 'steam_appid.txt') -Value '2288340' -Encoding ASCII
 Write-AC8Json $state $statePath
 $stage='核对部署的 DLL'
 $blocked=@(Get-AC8MissingDeployedDlls $payload $w64)
 if($blocked.Count){Stop-AC8Problem 'AV_BLOCKED' ('刚部署的 DLL 缺失或被改动：'+($blocked -join '、')) '很可能被安全软件隔离或拦截。请在 Windows 安全中心 → 病毒和威胁防护 → 保护历史中查看；确认是误报后恢复文件，必要时把游戏的 Win64 目录加入排除项，再重新启动。不要关闭安全软件。'}
 $env:SteamAppId='2288340';$env:SteamGameId='2288340';$env:EOS_USE_ANTICHEATCLIENTNULL='1'
 $banner=Get-AC8ConsoleBanner $root $validation
 Write-Host $banner.Title
 Write-Host $banner.Keys
 Write-Host ('FEATURES: mouse flight; missile mode='+$features.MissileMode)
 $usableExtra=@($extraMods | Where-Object {!$_.Problem})
 if($extraMods.Count){Write-Host ('EXTRA UE4SS MODS (ue4ss-mods): '+$(if($usableExtra.Count){($usableExtra | ForEach-Object {$_.Name+' ['+$_.Kind+']'}) -join ', '}else{'none loaded'}))}
 foreach($skipped in @($extraMods | Where-Object {$_.Problem})){Write-Host ('  skipped '+$skipped.Name+': '+$skipped.Problem) -ForegroundColor Yellow}
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
  Write-Host $banner.Ready
 }elseif(!$game.HasExited){
  Write-Host '[NOT READY] The selected feature set has not reported readiness. Check the archived log.'
  Write-Host ('  '+(Get-AC8ReadinessDiagnosis $runtimeLog $features @(Get-AC8MissingDeployedDlls $payload $w64) ([bool]@($extraMods | Where-Object {!$_.Problem}).Count))) -ForegroundColor Yellow
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

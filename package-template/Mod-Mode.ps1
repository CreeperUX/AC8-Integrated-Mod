param([ValidateSet('Disable','Enable','CheckOriginal')][string]$Action='Disable',[string]$Root=$PSScriptRoot)
$ErrorActionPreference='Stop'
foreach($module in @('Microsoft.PowerShell.Management','Microsoft.PowerShell.Utility')){Import-Module ([IO.Path]::Combine($PSHOME,'Modules',$module,($module+'.psd1'))) -ErrorAction Stop}
try {
 $Root=[IO.Path]::GetFullPath($Root)
 $parentRoot=Split-Path $Root -Parent
 $parentRegistry=Join-Path $parentRoot 'AC8-Managed-Packages.json'
 if(Test-Path -LiteralPath $parentRegistry){
  $parentData=Get-Content -LiteralPath $parentRegistry -Raw -Encoding UTF8|ConvertFrom-Json
  if(@($parentData.packages|Where-Object name -eq (Split-Path $Root -Leaf)).Count -eq 1){$Root=$parentRoot}
 }
 $registryPath=Join-Path $Root 'AC8-Managed-Packages.json';$managed=Test-Path -LiteralPath $registryPath
 $registry=$null;$package=$Root
 if($managed){
  $registry=Get-Content -LiteralPath $registryPath -Raw -Encoding UTF8|ConvertFrom-Json
  if($registry.activePackage -notmatch '^AC8-Missiles-MouseAim-v[0-9.]+-(?:[a-z]+-)*candidate$'){throw 'Invalid managed package.'}
  $package=Join-Path $Root $registry.activePackage
 }
 . (Join-Path $package 'Install-Common.ps1')
 $game=Resolve-AC8GameRoot ([IO.File]::ReadAllText((Join-Path $package 'game-path.txt')).Trim())
 $w64=Join-Path $game 'Game/Binaries/Win64';$flag=Join-Path $Root 'mod-disabled.flag'
 function Assert-Closed {
  if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw '请先正常退出游戏；不能在游戏运行时切换到原版。F8不是卸载。'}
  if(Get-Process start_protected_game -ErrorAction SilentlyContinue){throw '受保护启动程序仍在运行，请等游戏启动或退出流程结束后再切换。'}
  if(Get-Process ac8_screen_input -ErrorAction SilentlyContinue){throw '鼠标采集辅助进程尚未退出，请稍等后重试。'}
 }
 function Assert-Clean {
  foreach($name in @('dwmapi.dll','ue4ss','steam_appid.txt')){
   if(Test-Path -LiteralPath (Join-Path $w64 $name)){throw "仍有加载器文件 $name；拒绝原版启动。请先完成旧会话清理。"}
  }
 }
 Assert-Closed
 if($Action -eq 'Disable'){
  # Cleanup only identified ownership. Never delete unknown files to make this check pass.
  $marker=Join-Path $w64 'ue4ss/AC8SourceInit-owner.txt'
  if(Test-Path -LiteralPath $marker){
   $owner=[IO.File]::ReadAllText($marker).Trim();$candidates=@()
   $entries=if($managed){@($registry.packages)}else{@([pscustomobject]@{name='';launcherSHA256=$null})}
   foreach($entry in $entries){
    if($managed -and $entry.name -notmatch '^AC8-Missiles-MouseAim-v[0-9.]+-(?:[a-z]+-)*candidate$'){throw 'Invalid registered path.'}
    $folder=if($managed){Join-Path $Root $entry.name}else{$package}
    $stateFile=Join-Path $folder 'active-session.json'
    if(Test-Path -LiteralPath $stateFile){
     $state=Get-Content -LiteralPath $stateFile -Raw -Encoding UTF8|ConvertFrom-Json
     if($state.Id -eq $owner -and [IO.Path]::GetFullPath($state.Win64) -eq [IO.Path]::GetFullPath($w64)){
      $launcher=Join-Path $folder 'Launch-Offline.ps1'
      if($managed -and (Get-AC8FileHash -LiteralPath $launcher).Hash -ne $entry.launcherSHA256){throw 'Managed cleanup launcher changed.'}
      $candidates+=,$launcher
     }
    }
   }
   if($candidates.Count -ne 1){throw '无法唯一确认残留文件归属，请用原安装包的清理工具处理。'}
   & (Join-Path $PSHOME 'powershell.exe') -NoProfile -ExecutionPolicy Bypass -File $candidates[0] -CleanupOnly
   if($LASTEXITCODE -ne 0){throw '清理失败，未切换启动模式。'}
  }
 }
 $operation=Enter-AC8Operation $game
 try {
  Assert-Closed;Assert-Clean
  if(Test-Path -LiteralPath $flag){if((Get-Item -LiteralPath $flag -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unexpected mode marker link.'}}
  if($Action -eq 'Disable'){
   [IO.File]::WriteAllText($flag,'disabled',[Text.Encoding]::ASCII)
   Write-Host '本项目Mod已停用，已核验本项目加载器残留。下次从Steam执行原始启动命令。'
   Write-Host '联机前请确认其他Mod也已停用；本检查不是反作弊兼容性或账号安全保证。'
  }elseif($Action -eq 'Enable'){
   if(Test-Path -LiteralPath $flag){Remove-Item -LiteralPath $flag -Force}
   Write-Host '已恢复Mod启动。仅限离线单人，禁止用于多人。'
  }else{
   if(!(Test-Path -LiteralPath $flag -PathType Leaf)){throw 'Mod尚未停用。'}
   Write-Host '本项目加载器检查通过，交还Steam原始启动命令。'
  }
 }finally{Exit-AC8Operation $operation}
}catch{Write-Error $_ -ErrorAction Continue;exit 1}

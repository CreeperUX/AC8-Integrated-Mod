. (Join-Path $PSScriptRoot 'Install-Common.ps1')
. (Join-Path $PSScriptRoot 'Cleanup-Core.ps1')
. (Join-Path $PSScriptRoot 'Feature-Settings.ps1')
. (Join-Path $PSScriptRoot 'Discover-Installations.ps1')
function Get-AC8GuiSavedPath([string]$Root,[string]$Name){
 $path=Join-Path (Get-AC8DataRoot $Root) $Name
 if(Test-Path -LiteralPath $path -PathType Leaf){$text=Get-Content -LiteralPath $path -Raw -Encoding UTF8;if($null -ne $text){return $text.Trim()}}
 return ''
}
function Save-AC8GuiSettings([string]$Root,[string]$GameRoot,[string]$Steam,[ValidateSet('guidance','full','none')][string]$MissileMode,[string]$VmGamePath=''){
 $option='"'+(Get-AC8SteamEntry $Root)+'" %command%'
 $data=Get-AC8DataRoot $Root
 $values=[ordered]@{
  'game-path.txt'=$GameRoot
  'features.ini'=('missile_mode='+$MissileMode)
  'Steam-Launch-Option.txt'=$option
  'vm-game-path.txt'=$VmGamePath
 }
 $original=@{};$temporary=@{}
 try {
  foreach($name in $values.Keys){
   $path=Join-Path $data $name
   $exists=Test-Path -LiteralPath $path
   $original[$name]=[pscustomobject]@{Exists=$exists;Content=$(if($exists){[Convert]::ToBase64String([IO.File]::ReadAllBytes($path))}else{''})}
   $temporary[$name]=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
   Set-Content -LiteralPath $temporary[$name] -Value $values[$name] -Encoding UTF8
  }
  foreach($name in $values.Keys){Move-Item -LiteralPath $temporary[$name] -Destination (Join-Path $data $name) -Force}
 }catch{
  foreach($name in $original.Keys){
   $path=Join-Path $data $name
   if($original[$name].Exists){[IO.File]::WriteAllBytes($path,[Convert]::FromBase64String([string]$original[$name].Content))}
   elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
  }
  throw
 }finally{foreach($path in $temporary.Values){if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}}
 return $option
}
function Save-AC8SteamPath([string]$Root,[string]$Steam){
 # Steam 路径独立持久化：只写 steam-path.txt 一个文件，原子替换、失败即抛，不影响其他设置。
 $data=Get-AC8DataRoot $Root
 $path=Join-Path $data 'steam-path.txt'
 $temporary=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
 Set-Content -LiteralPath $temporary -Value $Steam -Encoding UTF8
 Move-Item -LiteralPath $temporary -Destination $path -Force
}
function Invoke-AC8GuiAction {
 param([ValidateSet('Discover','Check','Save','SaveSteam','Start','StartOffline','Recover','UpdateCheck','UpdateAuto')][string]$Action,[string]$Root,[string]$GamePath,[string]$SteamPath,[ValidateSet('guidance','full','none')][string]$MissileMode,[bool]$ConfirmRecovery=$false,[string]$VmGamePath='')
 if($Action -in @('UpdateCheck','UpdateAuto')){return Invoke-AC8GuiUpdateCheck $Root ($Action -eq 'UpdateAuto')}
 if($Action -eq 'Discover'){
  $found=Find-AC8Installations -GamePath $GamePath -SteamPath $SteamPath
  $result=[pscustomobject]@{Success=$true;Title='自动定位完成';Message='未能唯一确定位置。请选择下方候选，或使用浏览按钮手动选择。';GameRoot=$found.GamePath;SteamPath=$found.SteamPath;GameCandidates=@($found.GameCandidates);SteamCandidates=@($found.SteamCandidates);Option=$null}
  if($found.GamePath -and $found.SteamPath){
   try{
    $checked=Invoke-AC8GuiAction Check $Root $found.GamePath $found.SteamPath $MissileMode
    $result.Title='环境检查完成';$result.Message=$checked.Message;$result.GameRoot=$checked.GameRoot
   }catch{
    $result.Success=$false;$result.Title='定位完成，环境检查未通过'
    $result.Message=$_.Exception.Message+' '+[string]$_.Exception.Data['AC8Hint']
   }
  }
  if($found.Warnings.Count){$result.Message+=' '+($found.Warnings -join ' ')}
  return $result
 }
 if($Action -eq 'SaveSteam'){
  # Steam 路径独立保存：只处理 steam-path.txt，不要求游戏路径、不触发其他校验。
  $steamInput=$SteamPath.Trim().Trim('"')
  if($steamInput){
   if(!(Test-Path -LiteralPath $steamInput -PathType Leaf) -or [IO.Path]::GetFileName($steamInput) -ine 'steam.exe'){
    Stop-AC8Problem 'STEAM_PATH' '请选择有效的 steam.exe。' '点击 Steam 路径右侧的选择按钮，找到 Steam 安装目录中的 steam.exe。'
   }
   $steamInput=[IO.Path]::GetFullPath($steamInput)
  }
  Assert-AC8WriteAccess (Get-AC8DataRoot $Root)
  Save-AC8SteamPath $Root $steamInput
  return [pscustomobject]@{Success=$true;Title='Steam 路径已保存';Message=$(if($steamInput){'已保存 Steam 程序位置：'+$steamInput}else{'已清空 Steam 路径记录。'});GameRoot=$null;Option=$null}
 }
 if($Action -eq 'StartOffline'){
  # 虚拟机启动：完全不依赖 Steam 和上方“游戏位置”。填写虚拟机游戏位置时只用该副本，否则使用上方游戏位置。
  if($VmGamePath -and $VmGamePath.Trim()){
   $game=Resolve-AC8GameRoot $VmGamePath
  }else{
   $game=Resolve-AC8GameRoot $GamePath
  }
  Assert-AC8PackageLocation $Root $game
  Assert-AC8NoLinks $game
  & (Join-Path $Root 'Check-Package.ps1') -PackageRoot $Root
  $exe=Join-Path $game 'Game/Binaries/Win64/AceCombat8.exe'
  if((Get-AC8FileHash -LiteralPath $exe).Hash -ne '51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'){
   Stop-AC8Problem 'GAME_BUILD' '游戏版本与本包适配版本不一致。' '本包适配 Build 25201480，请核对游戏版本。'
  }
  $residuals=@(Get-AC8Residuals $game)
  $active=Test-Path -LiteralPath (Join-Path (Get-AC8DataRoot $Root) 'active-session.json')
  if([bool](Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue)){Stop-AC8Problem 'GAME_RUNNING' '游戏正在运行。' '请正常退出游戏，等待清理完成后再操作。'}
  if($active -or $residuals.Count){Stop-AC8Problem 'LOADER_CONFLICT' ('存在未完成会话或加载器残留：'+($residuals -join '、')) '请先点击“备份并清理”，确认归属并完成清理后再操作。'}
  $savedFeatures=Read-FeatureSettings (Join-Path (Get-AC8DataRoot $Root) 'features.ini')
  if($savedFeatures.MissileMode -ne $MissileMode){Stop-AC8Problem 'UNSAVED' '当前选择尚未保存。' '先点击“保存设置”，再启动游戏。'}
  & (Join-Path $Root 'Start-ModSession.ps1') -AllowNoSteam -GameRoot $game
  return [pscustomobject]@{Success=$true;Title='已直接启动';Message=('Mod 已注入并启动游戏（未经 Steam）：'+$game+'。请保留启动控制台，游戏退出后会自动清理还原；云存档同步本次不可用。');GameRoot=$null;Option=$null}
 }
 $game=Resolve-AC8GameRoot $GamePath
 Assert-AC8PackageLocation $Root $game
 Assert-AC8NoLinks $game
 if($Action -eq 'Recover'){
  if(!$ConfirmRecovery){Stop-AC8Problem 'CONFIRM_REQUIRED' '清理尚未确认。' '请先确认这些残留属于本整合包。'}
  & (Join-Path $Root 'Recover-Cleanup.ps1') -GameRoot $game -RecoverHistorical
  return [pscustomobject]@{Success=$true;Title='清理完成';Message='已复核加载器残留。清理前的备份保存在 cleanup-backups；需要停用 Mod 时，还请清除 Steam 启动选项。';GameRoot=$game;Option=$null}
 }
 & (Join-Path $Root 'Check-Package.ps1') -PackageRoot $Root
 $exe=Join-Path $game 'Game/Binaries/Win64/AceCombat8.exe'
 if((Get-AC8FileHash -LiteralPath $exe).Hash -ne '51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'){
  Stop-AC8Problem 'GAME_BUILD' '游戏版本与本包适配版本不一致。' '本包适配 Build 25201480，请核对游戏版本。'
 }
 $steam=$SteamPath.Trim().Trim('"')
 if(!$steam -or !(Test-Path -LiteralPath $steam -PathType Leaf) -or [IO.Path]::GetFileName($steam) -ine 'steam.exe'){
  Stop-AC8Problem 'STEAM_PATH' '请选择有效的 steam.exe。' '点击 Steam 路径右侧的选择按钮，找到 Steam 安装目录中的 steam.exe。'
 }
 $steam=[IO.Path]::GetFullPath($steam)
 $residuals=@(Get-AC8Residuals $game)
 $active=Test-Path -LiteralPath (Join-Path (Get-AC8DataRoot $Root) 'active-session.json')
 $running=[bool](Get-Process -Name AceCombat8 -ErrorAction SilentlyContinue)
 if($Action -eq 'Check'){
  $message='安装包与游戏版本校验通过。'
  if($running){$message+=' 游戏正在运行，请正常退出后再保存或清理。'}
  elseif($active -or $residuals.Count){$message+=' 检测到未完成会话或残留：'+($residuals -join '、')+'。请先使用“备份并清理”。'}
  else{$message+=' 可选择安装内容，保存设置后复制 Steam 启动选项。'}
  return [pscustomobject]@{Success=$true;Title='检查完成';Message=$message;GameRoot=$game;Option=$null}
 }
 if($running){Stop-AC8Problem 'GAME_RUNNING' '游戏正在运行。' '请正常退出游戏，等待清理完成后再操作。'}
 if($active -or $residuals.Count){Stop-AC8Problem 'LOADER_CONFLICT' '存在未完成会话或加载器残留。' '请先点击“备份并清理”，确认归属并完成清理后再操作。'}
 if($Action -eq 'Save'){
  Assert-AC8WriteAccess (Get-AC8DataRoot $Root)
  Assert-AC8WriteAccess (Join-Path $game 'Game/Binaries/Win64')
  $option=Save-AC8GuiSettings $Root $game $steam $MissileMode $VmGamePath
   Save-AC8SteamPath $Root $steam
  return [pscustomobject]@{Success=$true;Title='设置已保存';Message=('已保存：'+(Get-AC8GuiModeLabel $MissileMode)+'。下次启动生效。首次设置或移动整合包后，请复制启动选项到 Steam。');GameRoot=$game;Option=$option}
 }
 $savedGame=Get-AC8GuiSavedPath $Root 'game-path.txt';$savedSteam=Get-AC8GuiSavedPath $Root 'steam-path.txt'
 $savedFeatures=Read-FeatureSettings (Join-Path (Get-AC8DataRoot $Root) 'features.ini')
 if($savedGame -ne $game -or $savedSteam -ne $steam -or $savedFeatures.MissileMode -ne $MissileMode){Stop-AC8Problem 'UNSAVED' '当前选择尚未保存。' '先点击“保存设置”，再启动游戏。'}
 & (Join-Path $Root 'Launch-AC8-via-Steam.ps1')
 return [pscustomobject]@{Success=$true;Title='已发送启动请求';Message='Steam 将执行云同步并启动游戏。是否成功进入游戏，请以 Steam 和游戏窗口为准；保留启动控制台以便退出后清理。';GameRoot=$game;Option=$null}
}

# Launcher app settings (2.5.0) in app-settings.json next to the player's other settings.
function Read-AC8AppSettings([string]$Root){
 $values=[ordered]@{auto_update_check=$true;last_update_check_utc=''}
 $path=Join-Path (Get-AC8DataRoot $Root) 'app-settings.json'
 if(Test-Path -LiteralPath $path -PathType Leaf){
  try{
   $saved=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
   if($null -ne $saved.auto_update_check){$values.auto_update_check=[bool]$saved.auto_update_check}
   if($saved.last_update_check_utc){$values.last_update_check_utc=[string]$saved.last_update_check_utc}
  }catch{}
 }
 return $values
}
function Save-AC8AppSettings([string]$Root,$Values){
 $path=Join-Path (Get-AC8DataRoot $Root) 'app-settings.json';$temporary=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
 ConvertTo-Json -InputObject ([pscustomobject]$Values) | Set-Content -LiteralPath $temporary -Encoding UTF8
 Move-Item -LiteralPath $temporary -Destination $path -Force
}
function Test-AC8UpdateCheckDue([string]$Root){
 # The automatic check runs at most once every 20 hours and can be switched off in the launcher.
 $settings=Read-AC8AppSettings $Root
 if(!$settings.auto_update_check){return $false}
 if(!$settings.last_update_check_utc){return $true}
 try{$last=[DateTime]::Parse($settings.last_update_check_utc,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)}catch{return $true}
 return ([DateTime]::UtcNow-$last.ToUniversalTime()).TotalHours -ge 20
}
function Invoke-AC8GuiUpdateCheck([string]$Root,[bool]$Quiet,[string]$Feed=''){
 # Reads only the latest GitHub release (nothing is uploaded or downloaded). Installing goes through Host.ps1, which
 # checks the signed manifest and the package hash. A failed automatic check stays silent.
 try{
  . (Join-Path $Root 'app\App-Core.ps1')
  $info=Get-Content -LiteralPath (Join-Path $Root 'package-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
  $current=[string]$info.version
  $settings=Read-AC8AppSettings $Root;$settings.last_update_check_utc=[DateTime]::UtcNow.ToString('o');Save-AC8AppSettings $Root $settings
  $release=Get-AC8ReleaseInfo $Feed
  $latest='';if([string]$release.tag_name -match '^v(.+)$'){$latest=$Matches[1]}
  if(!(Test-AC8AppVersionName $latest)){throw 'GitHub 返回的版本号无法识别。'}
  $installed=[bool](Get-AC8AppInfo $Root)
  if((Compare-AC8AppVersion $latest $current) -gt 0){
   $message=if($installed){'当前 '+$current+'。确认后会关闭启动器，在控制台中下载、校验签名并安装；设置、键位和记录都会保留。'}else{'当前 '+$current+'（便携版）。请到 GitHub 发布页下载新版本；安装版可以在启动器中一键更新。'}
   return [pscustomobject]@{Success=$true;Title=('发现新版本 '+$latest);Message=$message;GameRoot=$null;Option=$null;Update=$latest;Installed=$installed;Quiet=$Quiet;NotesUrl=[string]$release.html_url}
  }
  return [pscustomobject]@{Success=$true;Title='已是最新版本';Message=('当前 '+$current+' 是 GitHub 上的最新正式版。');GameRoot=$null;Option=$null;Update=$null;Installed=$installed;Quiet=$Quiet;NotesUrl=[string]$release.html_url}
 }catch{
  if($Quiet){return [pscustomobject]@{Success=$true;Title='';Message='';GameRoot=$null;Option=$null;Update=$null;Installed=$false;Quiet=$true;NotesUrl=''}}
  throw
 }
}

function Get-AC8GuiModeLabel([ValidateSet('guidance','full','none')][string]$Mode){
 switch($Mode){'guidance'{return '飞控 + 仅比例引导'} 'full'{return '飞控 + 完整导弹强化'} 'none'{return '仅鼠标飞控'}}
}
function Get-AC8GuiSelectedMode($Controls){
 if($Controls.GuidanceOnly.IsChecked){return 'guidance'}
 if($Controls.FullInstall.IsChecked){return 'full'}
 if($Controls.MouseOnly.IsChecked){return 'none'}
 Stop-AC8Problem 'MODE_REQUIRED' '请选择一种安装范围。' '三种范围均包含鼠标飞控，请选择其中一项。'
}
function Set-AC8GuiSelectedMode($Controls,[ValidateSet('guidance','full','none')][string]$Mode){
 $Controls.GuidanceOnly.IsChecked=($Mode -eq 'guidance')
 $Controls.FullInstall.IsChecked=($Mode -eq 'full')
 $Controls.MouseOnly.IsChecked=($Mode -eq 'none')
}

. (Join-Path $PSScriptRoot 'Install-Common.ps1')
. (Join-Path $PSScriptRoot 'Cleanup-Core.ps1')
. (Join-Path $PSScriptRoot 'Feature-Settings.ps1')
. (Join-Path $PSScriptRoot 'Discover-Installations.ps1')
function Get-AC8GuiSavedPath([string]$Root,[string]$Name){
 $path=Join-Path $Root $Name
 if(Test-Path -LiteralPath $path -PathType Leaf){$text=Get-Content -LiteralPath $path -Raw -Encoding UTF8;if($null -ne $text){return $text.Trim()}}
 return ''
}
function Save-AC8GuiSettings([string]$Root,[string]$GameRoot,[string]$Steam,[ValidateSet('guidance','full','none')][string]$MissileMode){
 $option='"'+(Join-Path $Root 'Start-AC8-From-Steam.cmd')+'" %command%'
 $values=[ordered]@{
  'game-path.txt'=$GameRoot
  'steam-path.txt'=$Steam
  'features.ini'=('missile_mode='+$MissileMode)
  'Steam-Launch-Option.txt'=$option
 }
 $original=@{};$temporary=@{}
 try {
  foreach($name in $values.Keys){
   $path=Join-Path $Root $name
   $exists=Test-Path -LiteralPath $path
   $original[$name]=[pscustomobject]@{Exists=$exists;Content=$(if($exists){[Convert]::ToBase64String([IO.File]::ReadAllBytes($path))}else{''})}
   $temporary[$name]=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
   Set-Content -LiteralPath $temporary[$name] -Value $values[$name] -Encoding UTF8
  }
  foreach($name in $values.Keys){Move-Item -LiteralPath $temporary[$name] -Destination (Join-Path $Root $name) -Force}
 }catch{
  foreach($name in $original.Keys){
   $path=Join-Path $Root $name
   if($original[$name].Exists){[IO.File]::WriteAllBytes($path,[Convert]::FromBase64String([string]$original[$name].Content))}
   elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
  }
  throw
 }finally{foreach($path in $temporary.Values){if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}}
 return $option
}
function Invoke-AC8GuiAction {
 param([ValidateSet('Discover','Check','Save','Start','Recover')][string]$Action,[string]$Root,[string]$GamePath,[string]$SteamPath,[ValidateSet('guidance','full','none')][string]$MissileMode,[bool]$ConfirmRecovery=$false)
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
 $active=Test-Path -LiteralPath (Join-Path $Root 'active-session.json')
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
  Assert-AC8WriteAccess $Root
  Assert-AC8WriteAccess (Join-Path $game 'Game/Binaries/Win64')
  $option=Save-AC8GuiSettings $Root $game $steam $MissileMode
  return [pscustomobject]@{Success=$true;Title='设置已保存';Message=('已保存：'+(Get-AC8GuiModeLabel $MissileMode)+'。下次启动生效。首次设置或移动整合包后，请复制启动选项到 Steam。');GameRoot=$game;Option=$option}
 }
 $savedGame=Get-AC8GuiSavedPath $Root 'game-path.txt';$savedSteam=Get-AC8GuiSavedPath $Root 'steam-path.txt'
 $savedFeatures=Read-FeatureSettings (Join-Path $Root 'features.ini')
 if($savedGame -ne $game -or $savedSteam -ne $steam -or $savedFeatures.MissileMode -ne $MissileMode){Stop-AC8Problem 'UNSAVED' '当前选择尚未保存。' '先点击“保存设置”，再启动游戏。'}
 & (Join-Path $Root 'Launch-AC8-via-Steam.ps1')
 return [pscustomobject]@{Success=$true;Title='已发送启动请求';Message='Steam 将执行云同步并启动游戏。是否成功进入游戏，请以 Steam 和游戏窗口为准；保留启动控制台以便退出后清理。';GameRoot=$game;Option=$null}
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

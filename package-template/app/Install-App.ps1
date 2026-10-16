# AC8 Integrated setup (2.5.0): installs the release zip next to this script as a per-user app in
# %LOCALAPPDATA%\Programs\AC8 Integrated. No administrator rights; the game folder and Steam's settings are not changed
# (the launch option is shown and copied for the player to paste). Running it again repairs or updates the installation
# and keeps the player data. A portable package found through the current Steam launch option can be imported.
param([string]$Zip='',[string]$InstallRoot='',[string]$ImportFrom='',[switch]$NoImport,[switch]$Yes,[switch]$NoRegister,[switch]$NoLaunch,[string]$SteamRoot='')
$ErrorActionPreference='Stop'
foreach($module in @('Microsoft.PowerShell.Management','Microsoft.PowerShell.Utility')){Import-Module ([IO.Path]::Combine($PSHOME,'Modules',$module,($module+'.psd1'))) -ErrorAction Stop}
. ([IO.Path]::Combine($PSScriptRoot,'App-Core.ps1'))
try{$Host.UI.RawUI.WindowTitle='AC8 Integrated · 安装'}catch{}
try{
 Write-Host 'AC8 Integrated 安装程序 · 仅限离线单人'
 if(!$InstallRoot){$InstallRoot=Get-AC8DefaultInstallRoot}
 $InstallRoot=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
 if(!$Zip){
  $zips=@(Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'AC8-Integrated-v*-share.zip' -File)
  if($zips.Count -ne 1){Stop-AC8App 'SETUP_PACKAGE' '找不到要安装的安装包（AC8-Integrated-v…-share.zip）。' '请重新下载安装程序。'}
  $Zip=$zips[0].FullName
 }
 Write-Host ('安装位置：'+$InstallRoot)
 $busy=Get-AC8AppBusyReason $InstallRoot;if($busy){Stop-AC8App 'APP_BUSY' $busy}
 $existing=$null;try{$existing=Read-AC8AppState $InstallRoot}catch{$existing=$null}
 $setupVersion=([regex]::Match([IO.Path]::GetFileName($Zip),'^AC8-Integrated-v(.+)-share\.zip$')).Groups[1].Value
 if($existing -and (Test-AC8AppVersionName $setupVersion) -and (Compare-AC8AppVersion $setupVersion ([string]$existing.current)) -lt 0){
  Write-Host ('已安装更新的版本 '+$existing.current+'，此安装程序是 '+$setupVersion+'。') -ForegroundColor Yellow
  if($Yes -or (Read-Host ('输入 Y 改为使用 '+$setupVersion+'；直接回车取消')) -ine 'Y'){Stop-AC8App 'SETUP_OLDER' '已取消，没有任何改动。'}
 }
 $import=''
 if(!$NoImport -and ![IO.Directory]::Exists([IO.Path]::Combine($InstallRoot,'data'))){
  $import=if($ImportFrom){[IO.Path]::GetFullPath($ImportFrom).TrimEnd('\')}else{Find-AC8PortablePackage $InstallRoot $SteamRoot}
  if($import){
   if(![IO.File]::Exists([IO.Path]::Combine($import,'MouseAim-Settings.ini'))){Stop-AC8App 'IMPORT_SOURCE' ('这里不是 AC8 Integrated 便携版：'+$import)}
   if([IO.File]::Exists([IO.Path]::Combine($import,'active-session.json'))){Stop-AC8App 'PORTABLE_SESSION' ('便携版还有未完成的游戏会话：'+$import) '请先用便携版完成清理（正常退出游戏，或运行其中的 Cleanup-Offline.cmd），再运行安装程序。'}
   Write-Host ('找到正在使用的便携版：'+$import)
   if(!$Yes -and !$ImportFrom -and (Read-Host '直接回车导入它的设置、键位、路径和附加 Mod（原文件不改动）；输入 N 不导入') -ieq 'N'){$import=''}
  }
 }
 Write-Host '正在校验并安装…'
 $version=Install-AC8AppVersion $InstallRoot $Zip
 $report=@(Complete-AC8AppInstall $InstallRoot $version 'setup' $Zip $import -NoRegister:$NoRegister)
 foreach($item in $report){Write-Host ('  '+$item.Message) -ForegroundColor $(if($item.Status -eq 'problem'){'Red'}else{'Gray'})}
 $option='"'+[IO.Path]::Combine($InstallRoot,'Start-AC8-From-Steam.cmd')+'" %command%'
 Write-Host ''
 Write-Host ('已安装 AC8 Integrated '+$version+'。默认飞控为 WAR，游戏中按 F4 可切换到 PEACE。') -ForegroundColor Green
 Write-Host '最后一步：把下面整行粘贴到 Steam → AC8 → 属性 → 通用 → 启动选项（替换原有内容）：'
 Write-Host $option -ForegroundColor Cyan
 try{Set-Clipboard -Value $option;Write-Host '（已复制到剪贴板）'}catch{}
 Write-Host '以后从开始菜单的“AC8 Integrated”打开启动器；更新、检查并修复也在启动器里，卸载在 Windows 设置 → 应用。'
 if($import){Write-Host ('便携版仍在原处：'+$import+'。确认安装版正常后可以自行删除（其中 sessions 保存着旧的飞行记录和存档备份）。')}
  Write-Host '如果您是虚拟机版：请直接按回车打开启动器，使用“从虚拟机游戏启动”板块选择虚拟机内的游戏目录（启动前需开启硬件虚拟化 VT-x / AMD-V）。' -ForegroundColor Yellow
 if(!$NoLaunch -and ($Yes -or (Read-Host '直接回车打开启动器；输入 N 结束') -ine 'N')){
  $launcher=[IO.Path]::Combine($InstallRoot,'AC8 Integrated.exe')
  if([IO.File]::Exists($launcher)){Start-Process -FilePath $launcher}
  else{Start-Process -FilePath ([IO.Path]::Combine($PSHOME,'powershell.exe')) -WindowStyle Hidden -ArgumentList @('-NoProfile','-STA','-WindowStyle','Hidden','-ExecutionPolicy','Bypass','-File',('"'+[IO.Path]::Combine($InstallRoot,'Host.ps1')+'"'),'-Action','Gui')}
 }
 exit 0
}catch{
 $code=[string]$_.Exception.Data['AC8Code']
 Write-Host ('[安装未完成] '+$_.Exception.Message+$(if($code){' ['+$code+']'}else{''})) -ForegroundColor Red
 $hint=[string]$_.Exception.Data['AC8Hint'];if($hint){Write-Host $hint -ForegroundColor Yellow}
 Write-Host '已安装的版本和玩家数据保持不变。'
 if(!$Yes){[void](Read-Host '按回车关闭')}
 exit 1
}

param([switch]$RenderPreview,[string]$PreviewPath,[switch]$SmokeTest,[ValidateSet('dark','light')][string]$Theme='dark',[switch]$NoDialog)
$ErrorActionPreference='Stop'
try {
. ([IO.Path]::Combine($PSScriptRoot,'PowerShell-Compat.ps1'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase,System.Windows.Forms
. (Join-Path $PSScriptRoot 'Gui-Core.ps1')
. (Join-Path $PSScriptRoot 'CreeperUX-Theme.ps1')
$reader=New-Object System.Xml.XmlNodeReader ([xml](Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Launcher-GUI.xaml') -Raw -Encoding UTF8))
$window=[Windows.Markup.XamlReader]::Load($reader)
# Fit the initial window to the available desktop; the content scales down instead of scrolling.
$workArea=[Windows.SystemParameters]::WorkArea
$window.MinWidth=[Math]::Min($window.MinWidth,[Math]::Max(480,$workArea.Width-24))
$window.MinHeight=[Math]::Min($window.MinHeight,[Math]::Max(360,$workArea.Height-24))
$window.Width=[Math]::Min($window.Width,[Math]::Max($window.MinWidth,$workArea.Width-24))
$window.Height=[Math]::Min($window.Height,[Math]::Max($window.MinHeight,$workArea.Height-24))
$script:themeMode=$Theme
Set-CreeperUXTheme $window $script:themeMode
$script:controls=@{}
foreach($name in 'GamePath','SteamPath','BrowseGame','BrowseSteam','GuidanceOnly','MouseOnly','FullInstall','Check','Save','Start','CopyOption','LaunchOption','Recover','OpenLogs','StatusTitle','StatusMessage','StatusCard','BusyBar','PreviewRoot','ThemeToggle','StatusMark','ModeDescription','Detect','GameCandidates','SteamCandidates','PackageVersion'){$script:controls[$name]=$window.FindName($name)}
$script:worker=$null;$script:async=$null;$script:option=''
$infoPath=Join-Path $PSScriptRoot 'package-info.json'
if(Test-Path -LiteralPath $infoPath){
 $info=Get-Content -LiteralPath $infoPath -Raw -Encoding UTF8 | ConvertFrom-Json
 if($info.version){$label=switch($info.status){'prerelease'{'预发布'} 'release'{'正式版'} default{'候选'}};$script:controls.PackageVersion.Text=[string]$info.version+' · '+$label}
}

function Set-AC8GuiStatus([string]$Title,[string]$Message,[bool]$Success=$true){
 $script:controls.StatusTitle.Text=$Title
 $script:controls.StatusMessage.Text=$Message
 $key=if($Success){'CxInfo'}else{'CxBad'}
 $script:controls.StatusMark.SetResourceReference([Windows.Controls.Border]::BackgroundProperty,$key)
}
function Set-AC8GuiBusy([bool]$Busy){
 foreach($name in 'GamePath','SteamPath','BrowseGame','BrowseSteam','GuidanceOnly','MouseOnly','FullInstall','Check','Detect','Save','Start','Recover','GameCandidates','SteamCandidates'){$script:controls[$name].IsEnabled=!$Busy}
 $script:controls.BusyBar.IsIndeterminate=[Windows.SystemParameters]::ClientAreaAnimation
 $script:controls.BusyBar.Value=50
 $script:controls.BusyBar.Visibility=if($Busy){'Visible'}else{'Collapsed'}
}
function Start-AC8GuiWork([string]$Action,[bool]$Confirm=$false){
 if($script:worker){return}
 Set-AC8GuiBusy $true
 Set-AC8GuiStatus '正在处理…' '请稍候，完成后会在这里显示结果。'
 try {
  $script:worker=[PowerShell]::Create()
  [void]$script:worker.AddCommand((Join-Path $PSScriptRoot 'Gui-Worker.ps1')).AddParameter('Action',$Action).AddParameter('Root',$PSScriptRoot).AddParameter('GamePath',$script:controls.GamePath.Text).AddParameter('SteamPath',$script:controls.SteamPath.Text).AddParameter('MissileMode',(Get-AC8GuiSelectedMode $script:controls)).AddParameter('ConfirmRecovery',$Confirm)
  $script:async=$script:worker.BeginInvoke()
  $timer.Start()
 }catch{
  if($script:worker){$script:worker.Dispose()};$script:worker=$null
  Set-AC8GuiBusy $false;Set-AC8GuiStatus '操作未完成' $_.Exception.Message $false
 }
}
$timer=New-Object Windows.Threading.DispatcherTimer
$timer.Interval=[TimeSpan]::FromMilliseconds(150)
$timer.Add_Tick({
 if(!$script:async -or !$script:async.IsCompleted){return}
 $timer.Stop()
 try{
  $results=@($script:worker.EndInvoke($script:async))
  if(!$results.Count){throw '后台操作没有返回结果，请检查诊断记录。'}
  $result=$results[-1]
  Set-AC8GuiStatus $result.Title $result.Message ([bool]$result.Success)
  if($result.GameRoot){$script:controls.GamePath.Text=$result.GameRoot}
  if($result.SteamPath){$script:controls.SteamPath.Text=$result.SteamPath}
  if($result.PSObject.Properties['GameCandidates']){
   foreach($name in 'GameCandidates','SteamCandidates'){
    $items=@($result.$name)
    $script:controls[$name].ItemsSource=$items
    $script:controls[$name].SelectedIndex=-1
    $script:controls[$name].Visibility=if($items.Count -gt 1){'Visible'}else{'Collapsed'}
   }
  }
  if($result.Option){$script:option=$result.Option;$script:controls.LaunchOption.Text=$script:option;$script:controls.CopyOption.IsEnabled=$true}
 }catch{Set-AC8GuiStatus '操作未完成' $_.Exception.Message $false}
 finally{
  $script:worker.Dispose();$script:worker=$null;$script:async=$null;Set-AC8GuiBusy $false
  if($SmokeTest -and $script:smokeFrame){$script:smokeFrame.Continue=$false}
 }
})
function Update-AC8GuiModeDescription {
 $mode=Get-AC8GuiSelectedMode $script:controls
 $script:controls.ModeDescription.Text=switch($mode){
  'guidance'{'仅调整比例引导，保留原版性能、近炸设置与外观。'}
  'full'{'启用比例引导、性能强化、近炸设置与外观替换。'}
  'none'{'仅安装鼠标飞控，所有导弹保持原版。'}
 }
}
foreach($name in 'GuidanceOnly','FullInstall','MouseOnly'){$script:controls[$name].Add_Checked({Update-AC8GuiModeDescription})}
$script:controls.ThemeToggle.Content=if($Theme -eq 'dark'){'切换浅色'}else{'切换深色'}
$script:controls.ThemeToggle.Add_Click({
 $script:themeMode=if($script:themeMode -eq 'dark'){'light'}else{'dark'}
 Set-CreeperUXTheme $window $script:themeMode
 $script:controls.ThemeToggle.Content=if($script:themeMode -eq 'dark'){'切换浅色'}else{'切换深色'}
})
$script:controls.BrowseGame.Add_Click({
 $dialog=New-Object System.Windows.Forms.FolderBrowserDialog
 $dialog.Description='选择 AC8 游戏文件夹（根目录或 Win64 均可）';$dialog.ShowNewFolderButton=$false
 try{if($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK){$script:controls.GamePath.Text=$dialog.SelectedPath}}finally{$dialog.Dispose()}
})
$script:controls.BrowseSteam.Add_Click({
 $dialog=New-Object Microsoft.Win32.OpenFileDialog
 $dialog.Title='选择 Steam 安装目录中的 steam.exe';$dialog.Filter='Steam 程序 (steam.exe)|steam.exe';$dialog.CheckFileExists=$true
 if($dialog.ShowDialog($window)){$script:controls.SteamPath.Text=$dialog.FileName}
})
$script:controls.Check.Add_Click({Start-AC8GuiWork 'Check'})
$script:controls.Detect.Add_Click({Start-AC8GuiWork 'Discover'})
$script:controls.GameCandidates.Add_SelectionChanged({
 if(!$script:worker -and $script:controls.GameCandidates.SelectedItem){
  $script:controls.GamePath.Text=[string]$script:controls.GameCandidates.SelectedItem
  Start-AC8GuiWork 'Discover'
 }
})
$script:controls.SteamCandidates.Add_SelectionChanged({
 if(!$script:worker -and $script:controls.SteamCandidates.SelectedItem){
  $script:controls.SteamPath.Text=[string]$script:controls.SteamCandidates.SelectedItem
  Start-AC8GuiWork 'Discover'
 }
})
$script:controls.Save.Add_Click({Start-AC8GuiWork 'Save'})
$script:controls.Start.Add_Click({Start-AC8GuiWork 'Start'})
$script:controls.Recover.Add_Click({
 $message="将检查当前游戏路径中的加载器，先备份校验再清理。`n`n请确认这些残留属于 AC8 Integrated。若安装过其他 Mod 或不清楚来源，请取消。`n`n游戏路径："+$script:controls.GamePath.Text
 if(Show-CreeperUXConfirmation $window $message){Start-AC8GuiWork 'Recover' $true}
})
$script:controls.CopyOption.Add_Click({
 try{[Windows.Clipboard]::SetText($script:option);Set-AC8GuiStatus '已复制启动选项' '请在 Steam → AC8 → 属性 → 通用 → 启动选项中粘贴。'}catch{Set-AC8GuiStatus '复制未完成' '剪贴板暂不可用，可直接选中上方文字复制。' $false}
})
$script:controls.OpenLogs.Add_Click({
 try{
  $folder=Join-Path $PSScriptRoot 'diagnostics'
  if(!(Test-Path -LiteralPath $folder)){Set-AC8GuiStatus '暂无诊断文件' '发生错误后，详细诊断会自动保存在这里。';return}
  Start-Process -FilePath 'explorer.exe' -ArgumentList ('"'+$folder+'"') -WindowStyle Normal
 }catch{Set-AC8GuiStatus '无法打开文件夹' $_.Exception.Message $false}
})
$window.Add_Closing({param($sender,$event)
 if($script:worker){$event.Cancel=$true;Set-AC8GuiStatus '请等待当前操作完成' '检查或清理仍在进行，完成后即可关闭窗口。'}
})
if($SmokeTest){
 # Exercise all three actual mutually-exclusive radio controls and legacy configuration migration.
 foreach($mode in 'guidance','full','none'){
  Set-AC8GuiSelectedMode $script:controls $mode
  if((Get-AC8GuiSelectedMode $script:controls) -ne $mode){throw 'GUI installation mode mapping failed'}
  $checked=@('GuidanceOnly','FullInstall','MouseOnly' | Where-Object {$script:controls[$_].IsChecked})
  if($checked.Count -ne 1 -or !$script:controls.ModeDescription.Text){throw 'GUI mode selection is not exclusive or lacks description'}
 }
 Set-AC8GuiSelectedMode $script:controls (Read-FeatureSettings (Join-Path $PSScriptRoot 'features.ini')).MissileMode
 # Verify the token adapter and actual theme button before the background-action smoke.
 $before=$script:themeMode
 $script:controls.ThemeToggle.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
 if($script:themeMode -eq $before){throw 'Theme toggle did not change mode'}
 if(![Windows.SystemParameters]::HighContrast){
  $expected=if($script:themeMode -eq 'light'){'#FFEFEBE5'}else{'#FF0D1114'}
  if($window.Resources['CxBg'].Color.ToString() -ne $expected){throw 'Theme background differs from kit token'}
 }
 $script:controls.ThemeToggle.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
 foreach($key in 'CxDisplay','CxMono'){
  $typeface=[Windows.Media.Typeface]::new($window.Resources[$key],[Windows.FontStyles]::Normal,[Windows.FontWeights]::Normal,[Windows.FontStretches]::Normal)
  $glyph=$null
  if(!$typeface.TryGetGlyphTypeface([ref]$glyph) -or $glyph.FontUri.LocalPath -notmatch 'ui[/\\]creeperux[/\\]fonts'){throw "Bundled font did not load: $key"}
 }
 # Exercise the real button, dispatcher timer and asynchronous worker without showing a window.
 $script:controls.GamePath.Text=Join-Path $PSScriptRoot '__missing_game_for_ui_test__'
 $script:smokeFrame=New-Object Windows.Threading.DispatcherFrame
 $timeout=New-Object Windows.Threading.DispatcherTimer
 $timeout.Interval=[TimeSpan]::FromSeconds(15)
 $timeout.Add_Tick({$script:smokeFrame.Continue=$false})
 $timeout.Start()
 $script:controls.Check.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
 if(!$script:worker -or $script:controls.Check.IsEnabled){throw 'GUI did not enter busy state'}
 [Windows.Threading.Dispatcher]::PushFrame($script:smokeFrame)
 $timeout.Stop();$timer.Stop()
 if($script:worker){$script:worker.Stop();$script:worker.Dispose();throw 'GUI worker timeout'}
 if(!$script:controls.Check.IsEnabled -or $script:controls.StatusTitle.Text -ne '操作未完成'){throw 'GUI did not render error and restore controls'}
 Write-Output 'PASS real GUI button, busy state, async dispatcher result and error presentation.'
 exit 0
}
if($RenderPreview){
 if(!$PreviewPath){throw 'RenderPreview requires PreviewPath'}
 $script:controls.GamePath.Text='D:\Games\ACE COMBAT 8'
 $script:controls.SteamPath.Text='C:\Steam\steam.exe'
 Set-AC8GuiSelectedMode $script:controls 'guidance'
 Update-AC8GuiModeDescription
 $script:controls.LaunchOption.Text='"D:\Mods\AC8-Integrated\Start-AC8-From-Steam.cmd" %command%'
 $script:controls.CopyOption.IsEnabled=$true
 Set-AC8GuiStatus '界面预览 · 尚未执行检查' '示例路径仅用于展示。选择实际游戏位置后，可检查环境并保存设置。'
 $content=$script:controls.PreviewRoot
 $content.Width=940;$content.Height=720
 $content.Measure([Windows.Size]::new(1000,780));$content.Arrange([Windows.Rect]::new(0,0,1000,780));$content.UpdateLayout()
 $bitmap=New-Object Windows.Media.Imaging.RenderTargetBitmap 1000,780,96,96,([Windows.Media.PixelFormats]::Pbgra32)
 $background=New-Object Windows.Media.DrawingVisual
 $drawing=$background.RenderOpen()
 $drawing.DrawRectangle($window.Resources['CxBg'],$null,[Windows.Rect]::new(0,0,1000,780));$drawing.Close()
 $bitmap.Render($background)
 $bitmap.Render($content)
 $encoder=New-Object Windows.Media.Imaging.PngBitmapEncoder
 $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
 $stream=[IO.File]::Create([IO.Path]::GetFullPath($PreviewPath));try{$encoder.Save($stream)}finally{$stream.Dispose()}
 Write-Output 'GUI preview rendered; no game or configuration changes.'
 exit 0
}
try{
 $script:controls.GamePath.Text=Get-AC8GuiSavedPath $PSScriptRoot 'game-path.txt'
 $script:controls.SteamPath.Text=Get-AC8GuiSavedPath $PSScriptRoot 'steam-path.txt'
 Set-AC8GuiSelectedMode $script:controls (Read-FeatureSettings (Join-Path $PSScriptRoot 'features.ini')).MissileMode
 Update-AC8GuiModeDescription
 $script:option=Get-AC8GuiSavedPath $PSScriptRoot 'Steam-Launch-Option.txt'
 if($script:option){$script:controls.LaunchOption.Text=$script:option;$script:controls.CopyOption.IsEnabled=$true}
}catch{Set-AC8GuiStatus '读取设置未完成' $_.Exception.Message $false}
# Prevent two GUI instances from writing the same package settings simultaneously.
$sha=[Security.Cryptography.SHA256]::Create()
try{$key=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($PSScriptRoot.ToLowerInvariant()))).Replace('-','')}finally{$sha.Dispose()}
$created=$false;$mutex=New-Object Threading.Mutex($true,('Local\AC8Gui-'+$key),[ref]$created)
if(!$created){[void][Windows.MessageBox]::Show('此整合包的玩家工具已经打开。','AC8');$mutex.Dispose();exit 0}
$script:autoDiscoveryStarted=$false
$window.Add_ContentRendered({if(!$script:autoDiscoveryStarted){$script:autoDiscoveryStarted=$true;Start-AC8GuiWork 'Discover'}})
try{[void]$window.ShowDialog()}finally{$timer.Stop();$mutex.ReleaseMutex();$mutex.Dispose()}

}catch{
 $failure=$_;$diagnostic=''
 try{
  $dir=[IO.Path]::Combine($PSScriptRoot,'diagnostics');[void][IO.Directory]::CreateDirectory($dir)
  $diagnostic=[IO.Path]::Combine($dir,('gui-startup-'+[guid]::NewGuid().ToString('N')+'.txt'))
  [IO.File]::WriteAllText($diagnostic,($failure.ToString()+[Environment]::NewLine+$failure.ScriptStackTrace),[Text.UTF8Encoding]::new($true))
 }catch{}
 $message='玩家工具未能启动：'+$failure.Exception.Message
 if($diagnostic){$message+=[Environment]::NewLine+'诊断文件：'+$diagnostic}
 [Console]::Error.WriteLine($message)
 if(!$NoDialog){
  try{
   [void][Reflection.Assembly]::Load('PresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35')
   [void][Windows.MessageBox]::Show($message,'AC8 启动失败')
  }catch{}
 }
 exit 1
}

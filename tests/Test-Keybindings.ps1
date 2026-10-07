$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent;$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Keybinding-Settings.ps1')
$fixture=Join-Path $repo ('native/test-runtime/keybindings-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$script:GameRunning=$false
function Get-Process {param([string]$Name,$ErrorAction)if($script:GameRunning){[pscustomobject]@{Id=1}}}
Copy-Item -LiteralPath (Join-Path $template 'Keybindings.ini') -Destination $fixture
$values=Read-AC8Keybindings (Join-Path $fixture 'Keybindings.ini');if($values.free_look -ne 'C' -or $values.pitch_up -ne 'S'){throw 'Default binding mismatch'}
$values.free_look='V';$values.free_look_zoom='MouseX1';$values.pitch_up='I';$values.pitch_down='K';$values.roll_left='J';$values.roll_right='L';$values.yaw_left='U';$values.yaw_right='O'
Save-AC8Keybindings $fixture $values
$saved=Read-AC8Keybindings (Join-Path $fixture 'Keybindings.ini');if($saved.free_look -ne 'V' -or $saved.yaw_right -ne 'O'){throw 'Saved values not retained'}
if(!(Test-Path -LiteralPath (Join-Path $fixture 'Keybindings.ini.bak'))){throw 'Missing backup'}
$mod=Join-Path $fixture 'mod';New-Item -ItemType Directory -Path $mod | Out-Null
Copy-Item -LiteralPath (Join-Path $template 'payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/config.ini') -Destination $mod
Apply-AC8Keybindings $saved $mod;Apply-AC8Keybindings $saved $mod
$runtime=Get-Content -LiteralPath (Join-Path $mod 'config.ini') -Raw
if(([regex]::Matches($runtime,'\[keybindings\]')).Count -ne 1 -or $runtime -notmatch 'free_look=86' -or $runtime -notmatch 'free_look_zoom=5' -or $runtime -notmatch 'pitch_up=73'){throw 'Runtime staging not numeric or duplicated'}
$before=[IO.File]::ReadAllBytes((Join-Path $fixture 'Keybindings.ini'))
$values.roll_left='I';$caught=$false;try{Save-AC8Keybindings $fixture $values}catch{$caught=$true};if(!$caught){throw 'Conflict accepted'};$values.roll_left='J'
$values.free_look='F5';$caught=$false;try{Save-AC8Keybindings $fixture $values}catch{$caught=$true};if(!$caught){throw 'Reserved key accepted'};$values.free_look='V'
$script:GameRunning=$true;$caught=$false;try{Save-AC8Keybindings $fixture $values}catch{$caught=$true};if(!$caught){throw 'Running-game save accepted'};$script:GameRunning=$false
if([Convert]::ToBase64String($before) -ne [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $fixture 'Keybindings.ini')))){throw 'Rejected save changed configuration'}
Set-Content -LiteralPath (Join-Path $fixture 'bad.ini') -Value "[bindings]`nfree_look=V`nfree_look=J" -Encoding ASCII
$caught=$false;try{Read-AC8Keybindings (Join-Path $fixture 'bad.ini')}catch{$caught=$true};if(!$caught){throw 'Duplicate key entry accepted'}
Write-Host 'PASS defaults/custom save, backup, numeric runtime staging, duplicate/reserved/conflict guards and game-running rejection'
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
. (Join-Path $template 'CreeperUX-Theme.ps1')
. (Join-Path $template 'Keybindings-GUI.ps1')
$ctx=New-AC8KeybindingsDialog $fixture $null 'dark'
if($ctx.Buttons.Count -ne 8){throw 'GUI missing actions'}
$click=[Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent)
$ctx.Buttons.free_look.RaiseEvent($click);if($ctx.State.Capture -ne 'free_look'){throw 'Capture not armed'}
Set-AC8CapturedBinding $ctx 66;if($ctx.State.Values.free_look -ne 'B'){throw 'Key capture not retained'}
$ctx.Save.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent));if(!$ctx.State.Saved -or (Read-AC8Keybindings (Join-Path $fixture 'Keybindings.ini')).free_look -ne 'B'){throw ('GUI save failed: '+$ctx.Status.Text)}
$ctx.Buttons.roll_left.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent));Set-AC8CapturedBinding $ctx 73;if($ctx.Save.IsEnabled){throw 'GUI conflict did not block save'}
$ctx.Window.FindName('RestoreBindings').RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent));if($ctx.State.Values.free_look -ne 'C' -or !$ctx.Save.IsEnabled){throw 'GUI restore-default failed'}
$ctx.Save.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent));if((Read-AC8Keybindings (Join-Path $fixture 'Keybindings.ini')).free_look -ne 'C'){throw 'GUI defaults not saved'}
foreach($theme in @('dark','light')){
 $ctx=New-AC8KeybindingsDialog $fixture $null $theme
 foreach($size in @(@(860,750),@(680,560))){
  $ctx.Window.Content.Measure([Windows.Size]::new($size[0],$size[1]));$ctx.Window.Content.Arrange([Windows.Rect]::new(0,0,$size[0],$size[1]));$ctx.Window.Content.UpdateLayout()
  if($ctx.Buttons.free_look.ActualWidth -le 0 -or $ctx.Save.ActualWidth -le 0){throw 'GUI layout did not measure'}
 }
}
$ctx=New-AC8KeybindingsDialog $fixture $null 'dark';$ctx.Window.Content.Measure([Windows.Size]::new(860,750));$ctx.Window.Content.Arrange([Windows.Rect]::new(0,0,860,750));$ctx.Window.Content.UpdateLayout()
$bitmap=[Windows.Media.Imaging.RenderTargetBitmap]::new(860,750,96,96,[Windows.Media.PixelFormats]::Pbgra32);$bitmap.Render($ctx.Window.Content)
$encoder=[Windows.Media.Imaging.PngBitmapEncoder]::new();$encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap));$file=[IO.File]::Create((Join-Path $repo 'native/test-runtime/keybindings-preview.png'));try{$encoder.Save($file)}finally{$file.Dispose()}
Write-Host 'PASS real WPF button capture, save/restore, conflict state, dark/light layout and render preview'

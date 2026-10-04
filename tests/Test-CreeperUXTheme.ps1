$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$repo=Split-Path $PSScriptRoot -Parent
$template=Join-Path $repo 'package-template'
. (Join-Path $template 'CreeperUX-Theme.ps1')
$source=Get-Content -LiteralPath (Join-Path $template 'ui/creeperux/SOURCE.json') -Raw | ConvertFrom-Json
$tokenPath=Join-Path $template 'ui/creeperux/tokens.json'
if((Get-FileHash -LiteralPath $tokenPath).Hash -ne $source.tokens_sha256){throw 'Upstream token snapshot changed'}
$tokens=Get-Content -LiteralPath $tokenPath -Raw | ConvertFrom-Json
$reader=New-Object Xml.XmlNodeReader ([xml](Get-Content -LiteralPath (Join-Path $template 'Launcher-GUI.xaml') -Raw -Encoding UTF8))
$window=[Windows.Markup.XamlReader]::Load($reader)
foreach($mode in 'dark','light'){
 Set-CreeperUXTheme $window $mode
 foreach($name in 'bg','text','accent','boundary'){
  $key='Cx'+(Get-Culture).TextInfo.ToTitleCase($name)
  if(![Windows.SystemParameters]::HighContrast -and $window.Resources[$key].Color -ne (Convert-CxColor $tokens.color.$name.$mode)){throw "Token mismatch: $mode $name"}
 }
 $dialog=Show-CreeperUXConfirmation $window 'Test confirmation: no cleanup is performed.' -PassThru
 $dialog.Content.Measure([Windows.Size]::new(510,400));$dialog.Content.Arrange([Windows.Rect]::new(0,0,510,400));$dialog.Content.UpdateLayout()
 $actions=$dialog.Content.Children[2]
 if(!$actions.Children[0].IsCancel -or !$actions.Children[0].IsDefault){throw 'Confirmation must default to cancellation'}
 if($actions.Children[1].Tag -ne 'primary'){throw 'Confirmation action lost kit style'}
 if($dialog.Background.Color -ne $window.Resources['CxPanel'].Color){throw 'Confirmation uses a different theme'}
}
Write-Host 'PASS upstream token hash, dark/light palette, themed confirmation and default cancellation.'
exit 0

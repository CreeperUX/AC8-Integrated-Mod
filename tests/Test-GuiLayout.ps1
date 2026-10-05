$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$repo=Split-Path $PSScriptRoot -Parent;$template=Join-Path $repo 'package-template'
. (Join-Path $template 'CreeperUX-Theme.ps1')
$xml=[xml](Get-Content -LiteralPath (Join-Path $template 'Launcher-GUI.xaml') -Raw -Encoding UTF8)
$manager=New-Object Xml.XmlNamespaceManager($xml.NameTable)
$manager.AddNamespace('w','http://schemas.microsoft.com/winfx/2006/xaml/presentation')
if($xml.SelectNodes('//w:ScrollViewer[@Grid.Row="1"]',$manager).Count){throw 'Main-page scrolling returned'}
$cases=0
foreach($theme in 'dark','light'){
 foreach($size in @(@(1040,740),@(880,660),@(760,500),@(1280,800))){
  foreach($expanded in $false,$true){
   $window=[Windows.Markup.XamlReader]::Load([Xml.XmlNodeReader]::new($xml))
   Set-CreeperUXTheme $window $theme
   $root=$window.FindName('PreviewRoot')
   if($root -isnot [Windows.Controls.Viewbox]){throw 'Expected fitted, non-scrolling root'}
   if($expanded){
    foreach($name in 'GameCandidates','SteamCandidates'){$window.FindName($name).Visibility='Visible'}
    $window.FindName('StatusMessage').Text=('Environment check failed. Select another directory and try again. '*8)
   }
   $root.Measure([Windows.Size]::new($size[0],$size[1]))
   $root.Arrange([Windows.Rect]::new(0,0,$size[0],$size[1]));$root.UpdateLayout()
   foreach($name in 'Save','Start','Recover','OpenLogs','StatusMessage','ThemeToggle'){
    $control=$window.FindName($name)
    $rect=$control.TransformToAncestor($root).TransformBounds([Windows.Rect]::new(0,0,$control.ActualWidth,$control.ActualHeight))
    if($rect.Width -le 0 -or $rect.Height -le 0 -or $rect.X -lt -1 -or $rect.Y -lt -1 -or $rect.Right -gt $root.ActualWidth+1 -or $rect.Bottom -gt $root.ActualHeight+1){throw "Clipped control: $theme $size $expanded $name"}
   }
   $cases++
  }
 }
}
Write-Host "PASS $cases no-scroll layouts: both themes, window sizes, multiple candidates and long status messages."
exit 0

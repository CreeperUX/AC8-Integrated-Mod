function Convert-CxColor([string]$Value){
 if($Value -match '^rgba\((\d+),\s*(\d+),\s*(\d+),\s*([.\d]+)\)$'){
  $alpha=[byte][Math]::Round(255*[double]::Parse($Matches[4],[Globalization.CultureInfo]::InvariantCulture))
  return [Windows.Media.Color]::FromArgb($alpha,[byte]$Matches[1],[byte]$Matches[2],[byte]$Matches[3])
 }
 return [Windows.Media.ColorConverter]::ConvertFromString($Value)
}
function Set-CreeperUXTheme($Window,[ValidateSet('dark','light')][string]$Mode){
 $tokens=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ui/creeperux/tokens.json') -Raw -Encoding UTF8 | ConvertFrom-Json
 foreach($property in $tokens.color.PSObject.Properties){
  if($property.Name -eq 'shadow'){continue}
  $key='Cx'+(($property.Name -split '-' | ForEach-Object {(Get-Culture).TextInfo.ToTitleCase($_)}) -join '')
  $color=Convert-CxColor $property.Value.$Mode
  $Window.Resources[$key]=[Windows.Media.SolidColorBrush]::new($color)
 }
 if([Windows.SystemParameters]::HighContrast){
  foreach($name in 'Bg','Deep','Panel','Raised','Hover','AccentSoft'){$Window.Resources['Cx'+$name]=[Windows.SystemColors]::WindowBrush}
  foreach($name in 'Text','Muted','Line','Boundary','Bad','Warn','Info','Good'){$Window.Resources['Cx'+$name]=[Windows.SystemColors]::WindowTextBrush}
  foreach($name in 'Accent','AccentHi','AccentMark','AccentText','Focus'){$Window.Resources['Cx'+$name]=[Windows.SystemColors]::HighlightBrush}
  $Window.Resources['CxOnAccent']=[Windows.SystemColors]::HighlightTextBrush
 }
 $fontRoot=[Uri]::new(((Join-Path $PSScriptRoot 'ui/creeperux/fonts').TrimEnd('\')+'\'))
 $Window.Resources['CxSans']=[Windows.Media.FontFamily]::new('Segoe UI, Microsoft YaHei UI')
 $Window.Resources['CxDisplay']=[Windows.Media.FontFamily]::new($fontRoot,'./#Chakra Petch SemiBold')
 $Window.Resources['CxMono']=[Windows.Media.FontFamily]::new($fontRoot,'./#Share Tech Mono')
}
function Show-CreeperUXConfirmation($Owner,[string]$Message,[switch]$PassThru){
 $dialog=[Windows.Window]::new()
 $dialog.Title='确认清理';$dialog.Width=510;$dialog.SizeToContent='Height';$dialog.ResizeMode='NoResize'
 if(!$PassThru){$dialog.Owner=$Owner}
 $dialog.WindowStartupLocation='CenterOwner';$dialog.ShowInTaskbar=$false
 $dialog.Resources.MergedDictionaries.Add($Owner.Resources)
 $dialog.SetResourceReference([Windows.Controls.Control]::BackgroundProperty,'CxPanel')
 $dialog.SetResourceReference([Windows.Controls.Control]::ForegroundProperty,'CxText')
 $dialog.SetResourceReference([Windows.Controls.Control]::FontFamilyProperty,'CxSans')
 $dialog.FontSize=13
 $panel=[Windows.Controls.StackPanel]::new();$panel.Margin=[Windows.Thickness]::new(24);$dialog.Content=$panel
 $title=[Windows.Controls.TextBlock]::new();$title.Text='备份并清理';$title.FontSize=22;$title.Margin=[Windows.Thickness]::new(0,0,0,16);[void]$panel.Children.Add($title)
 $body=[Windows.Controls.TextBlock]::new();$body.Text=$Message;$body.TextWrapping='Wrap';$body.LineHeight=21;[void]$panel.Children.Add($body)
 $actions=[Windows.Controls.StackPanel]::new();$actions.Orientation='Horizontal';$actions.HorizontalAlignment='Right';$actions.Margin=[Windows.Thickness]::new(0,24,0,0);[void]$panel.Children.Add($actions)
 $cancel=[Windows.Controls.Button]::new();$cancel.Content='取消';$cancel.IsCancel=$true;$cancel.IsDefault=$true;$cancel.Margin=[Windows.Thickness]::new(0,0,8,0);[void]$actions.Children.Add($cancel)
 $confirm=[Windows.Controls.Button]::new();$confirm.Content='确认归属并清理';$confirm.Tag='primary';[void]$actions.Children.Add($confirm)
 $confirm.Add_Click({param($sender,$event) [Windows.Window]::GetWindow($sender).DialogResult=$true})
 if($PassThru){return $dialog}
 return $dialog.ShowDialog() -eq $true
}

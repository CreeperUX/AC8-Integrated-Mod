$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Close AC8 normally before changing installed features.'}
. (Join-Path $PSScriptRoot 'Feature-Settings.ps1')
$file=Join-Path $PSScriptRoot 'features.ini';$previous=Read-FeatureSettings $file
$default=if($previous.MissileEnhancement){'1'}else{'2'}
Write-Host '1. Mouse flight + customized missile enhancements / 鼠标飞控 + 导弹强化'
Write-Host '2. Mouse flight only; original game missiles / 仅鼠标飞控，保留原版导弹'
$choice=Read-Host "Choose 1 or 2 (Enter keeps $default)"
if(!$choice){$choice=$default}
if($choice -notin @('1','2')){throw 'Enter 1 or 2. Selection was not changed.'}
$value=if($choice -eq '1'){'1'}else{'0'}
Set-Content -LiteralPath $file -Value @('; Next-launch selection. No running game files are changed.',"missile_enhancement=$value") -Encoding UTF8
Write-Host 'Selection saved for next launch. F4 flight-policy switching remains available in both modes.'

$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Close AC8 normally before changing installed features.'}
. (Join-Path $PSScriptRoot 'Feature-Settings.ps1')
$file=Join-Path $PSScriptRoot 'features.ini';$previous=Read-FeatureSettings $file
$default=if($previous.MissileEnhancement){'1'}else{'2'}
Write-Host '1. Mouse flight + customized missile enhancements / 鼠标飞控 + 导弹强化'
Write-Host '2. Mouse flight only; original game missiles / 仅鼠标飞控，保留原版导弹'
while($true){
 $choice=Read-Host "输入 1 或 2（回车保留 $default；Q 取消）"
 if(!$choice){$choice=$default}
 if($choice -in @('1','2')){break}
 if($choice -ieq 'Q'){throw 'Feature selection cancelled.'}
 Write-Host '请输入 1 或 2，设置尚未更改。' -ForegroundColor Yellow
}
$value=if($choice -eq '1'){'1'}else{'0'}
Set-Content -LiteralPath $file -Value @('; Next-launch selection. No running game files are changed.',"missile_enhancement=$value") -Encoding UTF8
Write-Host 'Selection saved for next launch. F4 flight-policy switching remains available in both modes.'

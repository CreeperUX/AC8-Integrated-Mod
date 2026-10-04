$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Close AC8 normally before changing installed features.'}
. (Join-Path $PSScriptRoot 'Feature-Settings.ps1')
$file=Join-Path $PSScriptRoot 'features.ini';$previous=Read-FeatureSettings $file
$default=switch($previous.MissileMode){'guidance'{'1'} 'full'{'2'} 'none'{'3'}}
Write-Host '1. 鼠标飞控 + 仅比例引导 / Mouse flight + guidance only'
Write-Host '   保留原版性能、锁定、伤害、装填、近炸设置和导弹外观。'
Write-Host '2. 鼠标飞控 + 完整导弹强化 / Mouse flight + full missile package'
Write-Host '   启用比例引导、性能强化、近炸设置及模型替换。'
Write-Host '3. 仅鼠标飞控 / Mouse flight only'
while($true){
 $choice=Read-Host "输入 1、2 或 3（回车保留 $default；Q 取消）"
 if(!$choice){$choice=$default}
 if($choice -in @('1','2','3')){break}
 if($choice -ieq 'Q'){throw 'Feature selection cancelled.'}
 Write-Host '请输入 1、2 或 3，设置尚未更改。' -ForegroundColor Yellow
}
$value=switch($choice){'1'{'guidance'} '2'{'full'} '3'{'none'}}
Set-Content -LiteralPath $file -Value @('; Next-launch selection. No running game files are changed.',"missile_mode=$value") -Encoding UTF8
Write-Host "已保存，下次启动生效：$value。F4 在三种模式中均用于切换飞控策略。"

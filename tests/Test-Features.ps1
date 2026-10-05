$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Feature-Settings.ps1')
$tempRoot=Join-Path $repo ('native/test-runtime/features-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$source=Join-Path $template 'payload/Game/Binaries/Win64/ue4ss'
foreach($mode in @('none','guidance','full')){
 $cfg=Join-Path $tempRoot "features-$mode.ini";Set-Content -LiteralPath $cfg -Value "missile_mode=$mode" -Encoding ASCII
 $selection=Read-FeatureSettings $cfg
 if($selection.MissileMode -ne $mode){throw 'Selection parse mismatch'}
 $enabled=$mode -ne 'none';$count=if($mode -eq 'guidance'){32}else{375}
 $dest=Join-Path $tempRoot "stage-$mode";New-Item -ItemType Directory -Path $dest | Out-Null
 Set-Content -LiteralPath (Join-Path $dest 'AC8SourceInit-owner.txt') -Value 'test-owner'
 Install-SelectedUE4SS $source $dest $selection
 if(!(Test-Path (Join-Path $dest 'Mods/AC8MouseAim/Scripts/main.lua'))){throw 'Mouse module missing'}
 if((Test-Path (Join-Path $dest 'Mods/AC8SourceInit')) -ne $enabled){throw 'Missile staging incorrect'}
 $mods=Get-Content (Join-Path $dest 'Mods/mods.txt') -Raw
 if(($mods -match 'AC8SourceInit : 1') -ne $enabled){throw 'Module list mismatch'}
 if($enabled){
  $lua=Get-Content (Join-Path $dest 'Mods/AC8SourceInit/Scripts/installation_mode.lua') -Raw
  if($lua.Trim() -ne "return '$mode'"){throw 'Wrong staged Lua mode'}
 }
 $pattern=Get-FeatureReadinessPattern $selection
 $line=if($enabled){"[AC8SourceInit] SOURCE_APPLIED mode=$mode reason=test verified=$count"}else{'[AC8MouseAim] Loaded. Offline controller will activate after entering a mission.'}
 if($line -notmatch $pattern){throw 'Readiness mismatch'}
 if($mode -eq 'guidance' -and '[AC8SourceInit] SOURCE_APPLIED mode=full verified=375' -match $pattern){throw 'Full result accepted as guidance'}
 if($mode -eq 'guidance' -and '[AC8SourceInit] SOURCE_APPLIED mode=guidance verified=320' -match $pattern){throw 'Wrong guidance count accepted'}
 $caught=$false;try{Install-SelectedUE4SS $source $dest $selection}catch{$caught=$true};if(!$caught){throw 'Dirty staging accepted'}
}
foreach($flag in @(0,1)){
 $cfg=Join-Path $tempRoot 'legacy.ini';Set-Content $cfg "missile_enhancement=$flag"
 $parsed=Read-FeatureSettings $cfg;$expected=if($flag){'full'}else{'none'}
 if($parsed.MissileMode -ne $expected){throw 'Legacy migration changed selection'}
}
$cfg=Join-Path $tempRoot 'mixed.ini';Set-Content $cfg @('missile_enhancement=1','missile_mode=guidance')
$caught=$false;try{$null=Read-FeatureSettings $cfg}catch{$caught=$true};if(!$caught){throw 'Mixed settings accepted'}
foreach($value in @('2','-1','true','0.5','other=1')){
 $cfg=Join-Path $tempRoot 'bad.ini';Set-Content -LiteralPath $cfg -Value "missile_enhancement=$value" -Encoding ASCII
 $caught=$false;try{$null=Read-FeatureSettings $cfg}catch{$caught=$true};if(!$caught){throw 'Invalid feature choice accepted'}
}
$errorsAll=@()
Get-ChildItem -LiteralPath $template -Filter *.ps1 -Recurse | ForEach-Object {
 $tokens=$null;$errors=$null;[Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$tokens,[ref]$errors)|Out-Null;$errorsAll+=@($errors)
}
if($errorsAll.Count){throw 'PowerShell syntax validation failed'}
Copy-Item -LiteralPath (Join-Path $template 'Choose-Features.ps1'),(Join-Path $template 'Feature-Settings.ps1') -Destination $tempRoot
function Get-Process { [CmdletBinding()] param([string]$Name) if($global:AC8TestRunning){[pscustomobject]@{Id=1}} }
function Read-Host { param([string]$Prompt)
 if($global:AC8TestChoice -eq 'bad'){$global:AC8TestChoice='Q';return 'bad'}
 return $global:AC8TestChoice
}
$global:AC8TestRunning=$false
foreach($choice in @('1','2','3','')){
 $global:AC8TestChoice=$choice;& (Join-Path $tempRoot 'Choose-Features.ps1')
 $actual=Read-FeatureSettings (Join-Path $tempRoot 'features.ini')
 $expected=switch($choice){'1'{'guidance'} '2'{'full'} default{'none'}}
 if($actual.MissileMode -ne $expected){throw 'Interactive selector saved wrong mode'}
}
$before=Get-Content (Join-Path $tempRoot 'features.ini') -Raw
$global:AC8TestChoice='bad';$caught=$false;try{& (Join-Path $tempRoot 'Choose-Features.ps1')}catch{$caught=$true}
if(!$caught -or (Get-Content (Join-Path $tempRoot 'features.ini') -Raw) -ne $before){throw 'Invalid interactive selection changed features'}
$global:AC8TestRunning=$true;$global:AC8TestChoice='2';$caught=$false;try{& (Join-Path $tempRoot 'Choose-Features.ps1')}catch{$caught=$true}
if(!$caught -or (Get-Content (Join-Path $tempRoot 'features.ini') -Raw) -ne $before){throw 'Selection changed while simulated game running'}
Write-Host 'PASS feature selection, actual none/guidance/full staging, module lists, readiness, dirty-directory refusal and PowerShell syntax. No game started.'


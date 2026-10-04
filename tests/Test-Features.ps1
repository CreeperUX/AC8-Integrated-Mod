$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Feature-Settings.ps1')
$tempRoot=Join-Path $repo ('native/test-runtime/features-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$source=Join-Path $template 'payload/Game/Binaries/Win64/ue4ss'
foreach($flag in @(0,1)){
 $cfg=Join-Path $tempRoot "features-$flag.ini";Set-Content -LiteralPath $cfg -Value "missile_enhancement=$flag" -Encoding ASCII
 $selection=Read-FeatureSettings $cfg
 if($selection.MissileEnhancement -ne ($flag -eq 1)){throw 'Selection parse mismatch'}
 $dest=Join-Path $tempRoot "stage-$flag";New-Item -ItemType Directory -Path $dest | Out-Null
 Set-Content -LiteralPath (Join-Path $dest 'AC8SourceInit-owner.txt') -Value 'test-owner'
 Install-SelectedUE4SS $source $dest $selection
 if(!(Test-Path (Join-Path $dest 'Mods/AC8MouseAim/Scripts/main.lua'))){throw 'Mouse module missing'}
 if((Test-Path (Join-Path $dest 'Mods/AC8SourceInit')) -ne ($flag -eq 1)){throw 'Missile module staging incorrect'}
 $mods=Get-Content (Join-Path $dest 'Mods/mods.txt') -Raw
 if(($mods -match 'AC8SourceInit : 1') -ne ($flag -eq 1)){throw 'Module list mismatch'}
 $pattern=Get-FeatureReadinessPattern $selection
 $line=if($flag){'[AC8SourceInit] SOURCE_APPLIED reason=test verified=375'}else{'[AC8MouseAim] Loaded. Offline controller will activate after entering a mission.'}
 if($line -notmatch $pattern){throw 'Readiness mismatch'}
 if(!$flag -and '[AC8SourceInit] SOURCE_APPLIED verified=375' -match $pattern){throw 'Mouse-only checks wrong readiness'}
 $caught=$false;try{Install-SelectedUE4SS $source $dest $selection}catch{$caught=$true};if(!$caught){throw 'Nonempty staging was accepted'}
}
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
foreach($choice in @('2','1')){
 $global:AC8TestChoice=$choice;& (Join-Path $tempRoot 'Choose-Features.ps1')
 $actual=Read-FeatureSettings (Join-Path $tempRoot 'features.ini')
 if($actual.MissileEnhancement -ne ($choice -eq '1')){throw 'Interactive selector saved wrong mode'}
}
$before=Get-Content (Join-Path $tempRoot 'features.ini') -Raw
$global:AC8TestChoice='bad';$caught=$false;try{& (Join-Path $tempRoot 'Choose-Features.ps1')}catch{$caught=$true}
if(!$caught -or (Get-Content (Join-Path $tempRoot 'features.ini') -Raw) -ne $before){throw 'Invalid interactive selection changed features'}
$global:AC8TestRunning=$true;$global:AC8TestChoice='2';$caught=$false;try{& (Join-Path $tempRoot 'Choose-Features.ps1')}catch{$caught=$true}
if(!$caught -or (Get-Content (Join-Path $tempRoot 'features.ini') -Raw) -ne $before){throw 'Selection changed while simulated game running'}
Write-Host 'PASS feature selection, actual mouse-only/full staging, module lists, readiness, dirty-directory refusal and PowerShell syntax. No game started.'


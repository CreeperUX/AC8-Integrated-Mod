$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
$repo=Split-Path $PSScriptRoot -Parent;$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Gui-Core.ps1')
function Get-Process {param($Name,$ErrorAction) return $null}
function Get-AC8FileHash {param($LiteralPath)
 if([IO.Path]::GetFileName($LiteralPath) -eq 'AceCombat8.exe'){return [pscustomobject]@{Hash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
}
$root=Join-Path $repo ('native/test-runtime/gui-'+[guid]::NewGuid().ToString('N'))
$package=Join-Path $root 'package';Copy-Item -LiteralPath $template -Destination $package -Recurse
foreach($name in 'tools','models'){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $package -Recurse}
Set-Content -LiteralPath (Join-Path $package 'package-info.json') -Value '{}'
$payload=Join-Path $package 'payload'
$manifest=@(Get-ChildItem -LiteralPath $payload -File -Recurse | ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($payload.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
ConvertTo-Json -InputObject $manifest -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'payload-manifest.json') -Encoding UTF8
$game=Join-Path $root '游戏 文件夹';$w64=Join-Path $game 'Game/Binaries/Win64'
New-Item -ItemType Directory -Path $w64 -Force | Out-Null
Set-Content -LiteralPath (Join-Path $w64 'AceCombat8.exe') -Value 'fixture only'
$steam=Join-Path $root 'steam.exe';Set-Content -LiteralPath $steam -Value 'fixture only'
$count=0
function Pass($text){$script:count++;Write-Host "PASS $text"}
function Expect-Code($action,$code){$caught=$false;try{& $action}catch{if($_.Exception.Data['AC8Code'] -ne $code){throw};$caught=$true};if(!$caught){throw "Expected $code"}}
if((Get-AC8GuiSavedPath $package 'game-path.txt') -ne '' -or (Get-AC8GuiSavedPath $package 'steam-path.txt') -ne ''){throw 'Empty first-run settings were not accepted'}
Pass 'first launch accepts empty path settings'
$before=@(Get-ChildItem -LiteralPath $game -Recurse -Force).Count
$result=Invoke-AC8GuiAction Check $package $w64 $steam 'none'
if(!$result.Success -or $result.GameRoot -ne $game -or @(Get-ChildItem -LiteralPath $game -Recurse -Force).Count -ne $before){throw 'GUI check wrote to game or failed normalization'}
Pass 'read-only environment check and root normalization'
& {
 $fixtureDiscovery=[pscustomobject]@{GamePath=$game;SteamPath=$steam;GameCandidates=@($game);SteamCandidates=@($steam);Warnings=@()}
 function Find-AC8Installations {param($GamePath,$SteamPath) return $fixtureDiscovery}
 $beforeSettings=Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw
 $result=Invoke-AC8GuiAction Discover $package '' '' 'full'
 if(!$result.Success -or $result.Title -ne '环境检查完成' -or $result.GameRoot -ne $fixtureDiscovery.GamePath -or $result.SteamPath -ne $fixtureDiscovery.SteamPath){throw 'Automatic discovery did not run environment checks'}
 if((Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw) -ne $beforeSettings){throw 'Automatic check saved settings'}
}
Pass 'automatic discovery feeds the existing read-only environment check'

foreach($mode in 'guidance','full','none'){
 $result=Invoke-AC8GuiAction Save $package $game $steam $mode
 $selection=Read-FeatureSettings (Join-Path $package 'features.ini')
 if(!$result.Success -or !$result.Option -or $selection.MissileMode -ne $mode){throw "GUI did not persist $mode"}
 if((Get-AC8GuiSavedPath $package 'game-path.txt') -ne $game){throw 'GUI game path not persisted'}
 if((Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw).Trim() -ne "missile_mode=$mode"){throw 'GUI did not write canonical mode'}
 $stage=Join-Path $root ('staged/'+$mode);New-Item -ItemType Directory -Path $stage -Force | Out-Null
 Install-SelectedUE4SS (Join-Path $payload 'Game/Binaries/Win64/ue4ss') $stage $selection
 $module=Join-Path $stage 'Mods/AC8SourceInit'
 if((Test-Path -LiteralPath $module) -ne ($mode -ne 'none')){throw 'Saved GUI mode staged wrong modules'}
 if($mode -ne 'none'){
  if((Get-Content -LiteralPath (Join-Path $module 'Scripts/installation_mode.lua') -Raw).Trim() -ne "return '$mode'"){throw 'GUI selection lost in Lua staging'}
 }
 Pass "GUI $mode save, readback and actual module staging"
}
Expect-Code {Invoke-AC8GuiAction Start $package $game $steam 'full'} 'UNSAVED'
$null=Invoke-AC8GuiAction Save $package $game $steam 'guidance'
Expect-Code {Invoke-AC8GuiAction Start $package $game $steam 'full'} 'UNSAVED'
$null=Invoke-AC8GuiAction Save $package $game $steam 'full'
Expect-Code {Invoke-AC8GuiAction Start $package $game $steam 'guidance'} 'UNSAVED'
Pass 'guidance/full are distinct even though both enable the missile module'
foreach($legacy in 0,1){
 Set-Content -LiteralPath (Join-Path $package 'features.ini') -Value "missile_enhancement=$legacy" -Encoding ASCII
 $mode=(Read-FeatureSettings (Join-Path $package 'features.ini')).MissileMode
 $expected=if($legacy){'full'}else{'none'}
 if($mode -ne $expected){throw 'Legacy selection changed'}
 $null=Invoke-AC8GuiAction Save $package $game $steam $mode
 if((Read-FeatureSettings (Join-Path $package 'features.ini')).MissileMode -ne $expected){throw 'GUI migration lost legacy selection'}
}
Pass 'legacy full/flight-only choices persist through GUI migration'
$beforeConfig=Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw
$caught=$false;try{Save-AC8GuiSettings $package $game $steam 'invalid'}catch{$caught=$true}
if(!$caught -or (Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw) -ne $beforeConfig){throw 'Invalid mode mutated configuration'}
Pass 'invalid mode rejected before configuration mutation'
& {function Get-Process {param($Name,$ErrorAction) return [pscustomobject]@{Id=123}};Expect-Code {Invoke-AC8GuiAction Save $package $game $steam 'none'} 'GAME_RUNNING'}
Pass 'running game blocks configuration mutation'
# Force partial settings replacement to fail; even originally-empty files must be restored byte-for-byte.
[IO.File]::WriteAllBytes((Join-Path $package 'steam-path.txt'),[byte[]]@())
$names=@('game-path.txt','steam-path.txt','features.ini','Steam-Launch-Option.txt')
$original=@{};foreach($name in $names){$original[$name]=[Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $package $name)))}
& {
 function Move-Item {param($LiteralPath,$Destination,[switch]$Force)
  if([IO.Path]::GetFileName($Destination) -eq 'features.ini'){throw 'TEST_CONFIG_REPLACE_FAILURE'}
  Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force
 }
 $caught=$false;try{Save-AC8GuiSettings $package 'other-game' 'other-steam' 'full'}catch{if($_.Exception.Message -ne 'TEST_CONFIG_REPLACE_FAILURE'){throw};$caught=$true}
 if(!$caught){throw 'Expected configuration write failure'}
}
foreach($name in $names){if([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $package $name))) -ne $original[$name]){throw "Rollback changed $name"}}
if(@(Get-ChildItem -LiteralPath $package -Filter '*.tmp').Count){throw 'Temporary config files leaked'}
Pass 'partial save failure restores all original settings including empty files'
$ue=Join-Path $w64 'ue4ss';New-Item -ItemType Directory -Path $ue | Out-Null
Set-Content -LiteralPath (Join-Path $ue 'AC8SourceInit-owner.txt') -Value ([guid]::NewGuid().ToString())
Expect-Code {Invoke-AC8GuiAction Recover $package $game $steam 'none'} 'CONFIRM_REQUIRED'
if(!(Test-Path -LiteralPath $ue)){throw 'Unconfirmed recovery mutated files'}
Pass 'recovery requires explicit confirmation'
$result=Invoke-AC8GuiAction Recover $package $game $steam 'none' $true
if(!$result.Success -or (Test-Path -LiteralPath $ue)){throw 'GUI recovery failed'}
Pass 'confirmed recovery uses verified cleanup core'
$ps=[PowerShell]::Create()
try{
 [void]$ps.AddCommand((Join-Path $package 'Gui-Worker.ps1')).AddParameter('Action','Check').AddParameter('Root',$package).AddParameter('GamePath',(Join-Path $root 'missing')).AddParameter('SteamPath',$steam).AddParameter('MissileMode','none')
 $pending=$ps.BeginInvoke();if(!$pending.AsyncWaitHandle.WaitOne(15000)){throw 'Worker stalled or requested input'}
 $result=@($ps.EndInvoke($pending))[-1]
 if($result.Success -or !$result.Message -or $result.Message -match 'CategoryInfo|FullyQualifiedErrorId'){throw 'Worker did not return friendly failure'}
}finally{$ps.Dispose()}
Pass 'real background runspace returns structured errors without prompts'
# Only a test stub receives the start call; no Steam process is launched.
Set-Content -LiteralPath (Join-Path $package 'Launch-AC8-via-Steam.ps1') -Value 'Write-Host TEST_STEAM_REQUEST' -Encoding UTF8
$null=Invoke-AC8GuiAction Save $package $game $steam 'none'
$result=Invoke-AC8GuiAction Start $package $game $steam 'none'
if(!$result.Success -or $result.Title -ne '已发送启动请求'){throw 'Start handoff result missing'}
Pass 'start action delegates to Steam bridge after matching saved settings'
& powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $package 'Launcher-GUI.ps1') -SmokeTest
if($LASTEXITCODE){throw 'Real GUI dispatcher smoke failed'}
Pass 'real WPF button, disabled busy state and asynchronous error presentation'

# A hidden GUI startup failure must leave a diagnostic and nonzero exit status.
Rename-Item -LiteralPath (Join-Path $package 'Gui-Core.ps1') -NewName 'Gui-Core.original'
$oldPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
try{$startupOutput=@(& powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $package 'Launcher-GUI.ps1') -NoDialog 2>&1);$startupCode=$LASTEXITCODE}finally{$ErrorActionPreference=$oldPreference}
Rename-Item -LiteralPath (Join-Path $package 'Gui-Core.original') -NewName 'Gui-Core.ps1'
if($startupCode -eq 0 -or !@(Get-ChildItem -LiteralPath (Join-Path $package 'diagnostics') -Filter 'gui-startup-*.txt').Count){throw 'Hidden startup failure had no diagnostic'}
Pass 'hidden GUI bootstrap errors are recorded and return failure'

$global:LASTEXITCODE=0

Write-Host "PASS $count GUI backend scenarios. No real game or Steam settings changed."
exit 0

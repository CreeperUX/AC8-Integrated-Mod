$ErrorActionPreference='Stop'
# All filesystem operations below target isolated fixtures, independent of real game processes.
function Get-Process {param($Name,$ErrorAction) return $null}
$repo=Split-Path $PSScriptRoot -Parent
$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Install-Common.ps1')
$root=Join-Path $repo ('native/test-runtime/install-ux-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $root '游戏 空格 [test]';$w64=Join-Path $game 'Game/Binaries/Win64';$exe=Join-Path $w64 'AceCombat8.exe'
New-Item -ItemType Directory -Path $w64 -Force | Out-Null
Set-Content -LiteralPath $exe -Value 'fixture, never execute'
$count=0
function Pass($Message){$script:count++;Write-Host "PASS $Message"}
function Expect-Code($Action,$Code){
 $caught=$false;try{& $Action}catch{if($_.Exception.Data['AC8Code'] -ne $Code){throw};$caught=$true}
 if(!$caught){throw "Expected $Code"}
}
foreach($path in @($game,$w64,$exe,('"'+$exe+'"'),($w64+'\'),(Join-Path $game 'Game/Binaries'))){
 if((Resolve-AC8GameRoot $path) -ne $game){throw "Wrong normalized path: $path"}
}
Pass 'root, Win64, EXE, quotes, trailing separator, Chinese, spaces and literal brackets'
Expect-Code {Resolve-AC8GameRoot (Join-Path $root 'missing')} 'GAME_PATH'
Pass 'bad game path gives actionable typed error'
Expect-Code {Assert-AC8PackageLocation (Join-Path $w64 'mod') $game} 'PACKAGE_LOCATION'
Assert-AC8PackageLocation ($game+'-mods') $game
Pass 'inside-game package refused; sibling sharing prefix accepted'
Assert-AC8WriteAccess $w64
if(@(Get-ChildItem -LiteralPath $w64 -Filter '.ac8-write-test-*').Count){throw 'Write probe leaked'}
Expect-Code {Assert-AC8WriteAccess (Join-Path $root 'missing-parent')} 'WRITE_ACCESS'
Pass 'write probe cleaned up; unwritable path diagnosed'
$package=Join-Path $root '整合包';Copy-Item -LiteralPath $template -Destination $package -Recurse
$fakeSteam=Join-Path $root 'steam.exe';Set-Content -LiteralPath $fakeSteam -Value 'fixture'
& {
 $global:AC8UXAnswers=New-Object 'System.Collections.Generic.Queue[string]'
 foreach($answer in @((Join-Path $root 'missing'),$w64,'bad','2')){$global:AC8UXAnswers.Enqueue($answer)}
 function Read-Host {param($Prompt) if(!$global:AC8UXAnswers.Count){throw 'Unexpected prompt'};return $global:AC8UXAnswers.Dequeue()}
 function Get-Process {param($Name,$ErrorAction) if($Name -eq 'steam'){return [pscustomobject]@{Path=$fakeSteam}}}
 function Get-ItemProperty {throw 'No registry access in fixture'}
 function Get-FileHash {param($LiteralPath)
  if($LiteralPath -eq $exe){return [pscustomobject]@{Hash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'}}
  Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
 }
 & (Join-Path $package 'Setup.ps1')
 if($global:AC8UXAnswers.Count){throw 'Setup did not consume expected correction inputs'}
}
if((Get-Content -LiteralPath (Join-Path $package 'game-path.txt') -Raw -Encoding UTF8).Trim() -ne $game){throw 'Setup persisted wrong root'}
if((Get-Content -LiteralPath (Join-Path $package 'features.ini') -Raw) -notmatch 'missile_enhancement=0'){throw 'Feature retry failed'}
Pass 'actual Setup retries wrong path and menu choice, then saves normalized configuration'
$ue=Join-Path $w64 'ue4ss';New-Item -ItemType Directory -Path $ue | Out-Null
Set-Content -LiteralPath (Join-Path $ue 'AC8SourceInit-owner.txt') -Value ([guid]::NewGuid().ToString())
& {function Read-Host {param($Prompt) return ''};Expect-Code {Repair-AC8ResidualsInteractive $package $game} 'LOADER_CONFLICT'}
if(!(Test-Path -LiteralPath $ue)){throw 'Cancelled recovery removed files'}
Pass 'residual recovery cancellation keeps existing files'
& {function Read-Host {param($Prompt) return 'RECOVER'};Repair-AC8ResidualsInteractive $package $game}
if(Test-Path -LiteralPath $ue){throw 'Confirmed recovery did not clear owned directory'}
if(!(Test-Path -LiteralPath (Join-Path $package 'cleanup-backups'))){throw 'Recovery backup absent'}
Pass 'confirmed historical recovery integrated with setup helper'
$fakePython=Join-Path $root 'python-fake.cmd'
@('@echo off','echo ModuleNotFoundError: No module named numpy 1>&2','exit /b 1') | Set-Content -LiteralPath $fakePython -Encoding ASCII
$output=& {
 function Get-Command {param($Name,$ErrorAction) return [pscustomobject]@{Source=$fakePython}}
 Invoke-AC8OptionalAnalysis $package $root
} 6>&1 | Out-String
if($output -match 'ModuleNotFoundError|Traceback' -or $output -notmatch 'NumPy'){throw 'Missing dependency traceback leaked or guidance absent'}
Pass 'missing NumPy yields one informational message with no traceback'
$output=& {function Get-Command {param($Name,$ErrorAction) return $null};Invoke-AC8OptionalAnalysis $package $root} 6>&1 | Out-String
if($output -notmatch 'Python'){throw 'Missing Python guidance absent'}
Pass 'no Python remains optional'
# Actual Windows PowerShell process: wrapper must return nonzero without raw stack output.
$output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $package 'Run-Console.ps1') -Action Analyze 2>&1) -join "`n"
if($LASTEXITCODE -ne 1 -or $output -match 'CategoryInfo|FullyQualifiedErrorId|CommandNotFoundException'){throw 'Console wrapper failed to format/carry failure'}
if($output -notmatch '还没有可以分析'){throw 'Console lost Chinese text'}
Pass 'real console wrapper renders Chinese guidance, no stack, exit 1'
# Malformed settings exercise the same wrapper without an interactive prompt.
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value (Join-Path $root 'missing') -Encoding UTF8
$output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $package 'Run-Console.ps1') -Action Session 2>&1) -join "`n"
if($LASTEXITCODE -ne 1 -or $output -match 'CategoryInfo|FullyQualifiedErrorId'){throw 'Session failure not propagated'}
Pass 'session failure preserves nonzero status through console entrypoint'
Write-Host "PASS $count installer UX scenarios. No real game or Steam settings changed."

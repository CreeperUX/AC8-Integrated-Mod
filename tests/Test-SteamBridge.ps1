$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$root=Join-Path $repo ('native/test-runtime/steam-'+[guid]::NewGuid().ToString('N'))
$package=Join-Path $root 'mod } with bracket [test]'
Copy-Item -LiteralPath (Join-Path $repo 'package-template') -Destination $package -Recurse
$steam=Join-Path $root 'Steam/steam.exe';New-Item -ItemType Directory -Path (Split-Path $steam) -Force | Out-Null
Set-Content -LiteralPath $steam -Value 'fixture'
Set-Content -LiteralPath (Join-Path $package 'steam-path.txt') -Value $steam -Encoding UTF8
$option='"'+(Join-Path $package 'Start-AC8-From-Steam.cmd')+'" %command%'
function Save-Option($Account,$Value){
 $dir=Join-Path (Split-Path $steam) ('userdata/'+$Account+'/config');New-Item -ItemType Directory -Path $dir -Force | Out-Null
 $escaped=$Value.Replace('\','\\').Replace('"','\"')
 Set-Content -LiteralPath (Join-Path $dir 'localconfig.vdf') -Value ('"apps" { "2288340" { "LaunchOptions" "'+$escaped+'" } }') -Encoding UTF8
}
function Get-Process {param($Name,$ErrorAction) return $null}
$global:AC8FixtureSteamAccount=123
function Get-ItemProperty {param($LiteralPath,$ErrorAction) return [pscustomobject]@{ActiveUser=$global:AC8FixtureSteamAccount}}
function Start-Process {throw 'Read-only Steam check launched a process'}
Save-Option 123 $option
& (Join-Path $package 'Launch-AC8-via-Steam.ps1') -CheckOnly
Write-Host 'PASS quoted path with unmatched brace and literal brackets'
Save-Option 456 'wrong launch option'
$global:AC8FixtureSteamAccount=456
$caught=$false;try{& (Join-Path $package 'Launch-AC8-via-Steam.ps1') -CheckOnly;if($LASTEXITCODE -ne 0){$caught=$true}}catch{$caught=$true}
if(!$caught){throw 'Inactive account launch option was accepted'}
$global:AC8FixtureSteamAccount=0
$caught=$false;try{& (Join-Path $package 'Launch-AC8-via-Steam.ps1') -CheckOnly;if($LASTEXITCODE -ne 0){$caught=$true}}catch{$caught=$true}
if(!$caught){throw 'Ambiguous account was accepted'}
Save-Option 456 $option
$global:AC8FixtureSteamAccount=456
& (Join-Path $package 'Launch-AC8-via-Steam.ps1') -CheckOnly
Save-Option 3221225473 $option
$global:AC8FixtureSteamAccount=-1073741823
& (Join-Path $package 'Launch-AC8-via-Steam.ps1') -CheckOnly
Write-Host 'PASS unsigned Steam account ID preserved from signed DWORD'
Write-Host 'PASS current-account selection, wrong/ambiguous rejection and valid account recovery'
$global:LASTEXITCODE=0

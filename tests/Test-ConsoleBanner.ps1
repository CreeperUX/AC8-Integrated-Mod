$ErrorActionPreference='Stop'
# Launch console banner (2.4.2): version and status come from package-info.json / validation-status.json.
$repo=Split-Path $PSScriptRoot -Parent;$template=Join-Path $repo 'package-template'
. (Join-Path $template 'Install-Common.ps1')
$fixture=Join-Path $repo ('native/test-runtime/banner-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $fixture -Force | Out-Null
function Banner($info,$validation){
 $p=Join-Path $fixture 'package-info.json';if($info){Set-Content -LiteralPath $p -Value ($info|ConvertTo-Json) -Encoding UTF8}elseif(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p}
 return Get-AC8ConsoleBanner $fixture $validation
}
$b=Banner ([pscustomobject]@{version='2.4.2';native_controller_version='2.4.2 (WAR 13.8)'}) ([pscustomobject]@{candidateOnly=$false})
if($b.Title -notlike 'AC8 Integrated Mod 2.4.2 (WAR 13.8) - F4 PEACE/WAR*' -or $b.Title -like '*candidate*' -or $b.Ready -like '*candidate*'){throw "release banner: $($b.Title) / $($b.Ready)"}
if($b.Keys -notlike '*F4 PEACE/WAR*' -or $b.Keys -notlike '*Alt+F7 gun cross*'){throw "keys line: $($b.Keys)"}
$c=Banner ([pscustomobject]@{version='2.3.17-keybindings-preview.1';native_controller_version='2.4.2-war.13.8'}) ([pscustomobject]@{candidateOnly=$true})
if($c.Title -notlike 'AC8 Integrated Mod 2.4.2-war.13.8 `[local candidate`]*' -or $c.Ready -notlike 'Local candidate build*'){throw "candidate banner: $($c.Title) / $($c.Ready)"}
$d=Banner ([pscustomobject]@{version='2.4.2'}) $null;if($d.Title -notlike 'AC8 Integrated Mod 2.4.2 - *'){throw "version fallback: $($d.Title)"}
$e=Banner $null $null;if($e.Title -notlike '*(unknown version)*'){throw "missing package-info: $($e.Title)"}
$launch=Get-Content -LiteralPath (Join-Path $template 'Launch-Offline.ps1') -Raw
if($launch -match '2\.3\.5|CLASSIC/AGILE|This is a candidate, not the completed release' -or $launch -notmatch 'Get-AC8ConsoleBanner'){throw 'Launch-Offline.ps1 still prints a fixed banner'}
Remove-Item -LiteralPath $fixture -Recurse -Force
Write-Host 'PASS console banner from package metadata (release, local candidate, version fallback, missing metadata); no fixed 2.3.5 CLASSIC/AGILE text'

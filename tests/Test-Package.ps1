$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$root=Join-Path $repo ('native/test-runtime/package-'+[guid]::NewGuid().ToString('N'))
Copy-Item -LiteralPath (Join-Path $repo 'package-template') -Destination $root -Recurse
foreach($name in @('tools','models')){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $root -Recurse}
Set-Content -LiteralPath (Join-Path $root 'package-info.json') -Value '{}'
$payload=Join-Path $root 'payload'
$manifest=@(Get-ChildItem -LiteralPath $payload -File -Recurse | ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($payload.Length+1);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
ConvertTo-Json -InputObject $manifest -Depth 4 | Set-Content (Join-Path $root 'payload-manifest.json')
& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root
$gate=Join-Path $root 'validation-status.json';$backup=Join-Path $root 'validation-status.original'
Rename-Item -LiteralPath $gate -NewName 'validation-status.original'
$caught=$false;try{& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root}catch{if($_ -notmatch 'validation-status.json'){throw};$caught=$true}
if(!$caught){throw 'Regression: missing startup gate was accepted'}
Set-Content -LiteralPath $gate -Value '{"deploymentAllowed":false}'
$caught=$false;try{& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root}catch{$caught=$true}
if(!$caught){throw 'Disabled deployment gate was accepted'}
Copy-Item -LiteralPath $backup -Destination $gate -Force
Add-Content -LiteralPath (Join-Path $payload 'Game/Binaries/Win64/ue4ss/Mods/mods.txt') -Value 'test tamper'
$caught=$false;try{& (Join-Path $root 'Check-Package.ps1') -PackageRoot $root}catch{$caught=$true}
if(!$caught){throw 'Tampered payload was accepted'}
Write-Host 'PASS package preflight; missing validation-status, disabled gate and changed payload all fail before installation.'

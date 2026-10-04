param([string]$PackageRoot=$PSScriptRoot)
$ErrorActionPreference='Stop'
function PackageSHA256([string]$Path){
 $stream=[IO.File]::OpenRead($Path);$sha=[Security.Cryptography.SHA256]::Create()
 try{return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}
 finally{$sha.Dispose();$stream.Dispose()}
}
foreach($name in @('validation-status.json','payload-manifest.json','package-info.json','Launch-Offline.ps1','Cleanup-Core.ps1','Recover-Cleanup.ps1','Recover-Cleanup.cmd','MouseAim-Settings.ps1','MouseAim-Settings.ini','Feature-Settings.ps1','features.ini','Choose-Features.ps1','game-path.txt','steam-path.txt','tools/analyze_experiment.py','models/f15e-shadow-v2.json')){
 if(!(Test-Path -LiteralPath (Join-Path $PackageRoot $name) -PathType Leaf)){throw "Incomplete package: missing $name. Extract a complete release package."}
}
$validation=Get-Content -LiteralPath (Join-Path $PackageRoot 'validation-status.json') -Raw | ConvertFrom-Json
if($validation.deploymentAllowed -ne $true){throw 'Package is gated off by validation-status.json.'}
$payload=[IO.Path]::GetFullPath((Join-Path $PackageRoot 'payload'))
$manifest=Get-Content -LiteralPath (Join-Path $PackageRoot 'payload-manifest.json') -Raw | ConvertFrom-Json
$actual=@(Get-ChildItem -LiteralPath $payload -File -Recurse)
if($actual.Count -ne $manifest.Count){throw 'Payload file count differs from manifest.'}
foreach($item in $manifest){
 $file=[IO.Path]::GetFullPath((Join-Path $payload $item.Path))
 if(!$file.StartsWith($payload+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload path escaped package.'}
 if(!(Test-Path -LiteralPath $file -PathType Leaf) -or (PackageSHA256 $file) -ne $item.SHA256){throw "Payload hash mismatch: $($item.Path)"}
}
. (Join-Path $PackageRoot 'MouseAim-Settings.ps1')
. (Join-Path $PackageRoot 'Feature-Settings.ps1')
$null=Read-MouseSettings (Join-Path $PackageRoot 'MouseAim-Settings.ini')
$null=Read-FeatureSettings (Join-Path $PackageRoot 'features.ini')
Write-Host 'PACKAGE CHECK passed: required files, deployment gate, payload hashes and settings. No game started.'

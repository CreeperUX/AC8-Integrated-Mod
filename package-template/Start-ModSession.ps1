$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
$launcher=Join-Path $PSScriptRoot 'Launch-Offline.ps1'
if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'active-session.json')){
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $launcher -CleanupOnly
 if($LASTEXITCODE -ne 0){throw 'Owned session cleanup failed. Nothing new launched.'}
}
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $launcher
exit $LASTEXITCODE

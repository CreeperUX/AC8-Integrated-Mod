param([ValidateSet('Setup','Start','Session','Cleanup','Recover','Features','Analyze')][string]$Action,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Install-Common.ps1')
try {
 if($CheckOnly -and $Action -ne 'Session'){throw 'CheckOnly is supported only for Session.'}
 switch($Action){
  'Setup' {& (Join-Path $PSScriptRoot 'Setup.ps1')}
  'Start' {& (Join-Path $PSScriptRoot 'Launch-AC8-via-Steam.ps1')}
  'Session' {& (Join-Path $PSScriptRoot 'Start-ModSession.ps1') -CheckOnly:$CheckOnly}
  'Cleanup' {& (Join-Path $PSScriptRoot 'Launch-Offline.ps1') -CleanupOnly}
  'Recover' {& (Join-Path $PSScriptRoot 'Recover-Cleanup.ps1') -Interactive}
  'Features' {& (Join-Path $PSScriptRoot 'Choose-Features.ps1')}
  'Analyze' {& (Join-Path $PSScriptRoot 'Analyze-Latest.ps1')}
 }
 exit 0
}catch{if($CheckOnly){Write-Host ('CHECK FAILED: '+$_.Exception.Message)}else{Show-AC8Problem $_ $PSScriptRoot $Action};exit 1}

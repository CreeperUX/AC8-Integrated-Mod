param([string]$GameRoot,[switch]$RecoverHistorical,[switch]$CheckOnly,[switch]$Interactive)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Cleanup-Core.ps1')
if(!$GameRoot -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'game-path.txt'))){$GameRoot=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'game-path.txt') -Raw -Encoding UTF8).Trim()}
if(!$GameRoot){$GameRoot=(Read-Host 'Paste the AC8 game root from Steam > Manage > Browse local files').Trim().Trim('"')}
$statePath=Join-Path $PSScriptRoot 'active-session.json'
$state=$null
if(Test-Path -LiteralPath $statePath){$state=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json}
if($Interactive -and !$CheckOnly -and !$state){
 Write-Host "Game root: $GameRoot"
 Write-Host 'Historical recovery backs up and removes recognized AC8 Integrated loader files only.'
 Write-Host 'If you installed another UE4SS mod or do not know the origin, cancel and review first.'
 if((Read-Host 'Confirm these leftovers belong to AC8 Integrated: type RECOVER') -cne 'RECOVER'){throw 'Recovery cancelled. No files changed.'}
 $RecoverHistorical=$true
}
$archive=Invoke-AC8Cleanup -GameRoot $GameRoot -BackupRoot (Join-Path $PSScriptRoot 'cleanup-backups') -State $state -RecoverHistorical:$RecoverHistorical -CheckOnly:$CheckOnly
if($state -and !$CheckOnly){
 # Retire the record independently of optional save/analysis diagnostics.
 if(!$archive){$archive=Join-Path $PSScriptRoot ('cleanup-backups/completed-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $archive -Force | Out-Null}
 Move-Item -LiteralPath $statePath -Destination (Join-Path $archive 'completed-session.json')
}

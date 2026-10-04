﻿$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Close AC8 before setup.'}
$root=$PSScriptRoot
$gameRoot=(Read-Host 'Paste AC8 folder from Steam > Manage > Browse local files').Trim().Trim('"')
if(!$gameRoot){throw 'No game folder entered.'}
$gameRoot=[IO.Path]::GetFullPath($gameRoot)
$exe=Join-Path $gameRoot 'Game/Binaries/Win64/AceCombat8.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'AceCombat8.exe not found in selected folder.'}
if((Get-FileHash -LiteralPath $exe).Hash -ne '51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'){throw 'Game build differs from the tested build25201480. Setup stopped.'}
$prefix=$gameRoot.TrimEnd('\')+'\'
if($root.Equals($gameRoot,[StringComparison]::OrdinalIgnoreCase) -or $root.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Extract this mod outside the game folder.'}
$candidates=@()
try{$steamPath=(Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath;if($steamPath){$candidates+=Join-Path $steamPath 'steam.exe'}}catch{}
try{$candidates+=@(Get-Process steam -ErrorAction Stop | ForEach-Object {$_.Path})}catch{}
$candidates+=Join-Path ${env:ProgramFiles(x86)} 'Steam/steam.exe'
$steam=@($candidates | Where-Object {$_ -and (Test-Path -LiteralPath $_ -PathType Leaf)}) | Select-Object -First 1
if(!$steam){$steam=(Read-Host 'Paste full path to steam.exe').Trim().Trim('"')}
if(!$steam -or !(Test-Path -LiteralPath $steam -PathType Leaf) -or [IO.Path]::GetFileName($steam) -ine 'steam.exe'){throw 'Invalid Steam executable path.'}
$steam=[IO.Path]::GetFullPath($steam)
& (Join-Path $root 'Choose-Features.ps1')
Set-Content -LiteralPath (Join-Path $root 'game-path.txt') -Value $gameRoot -Encoding UTF8
Set-Content -LiteralPath (Join-Path $root 'steam-path.txt') -Value $steam -Encoding UTF8
$option='"'+(Join-Path $root 'Start-AC8-From-Steam.cmd')+'" %command%'
Set-Content -LiteralPath (Join-Path $root 'Steam-Launch-Option.txt') -Value $option -Encoding UTF8
Write-Host 'Now paste this complete line into Steam > AC8 > Properties > General > Launch Options:'
Write-Host $option
Write-Host 'Then start from Steam. If Cloud conflicts, choose the local progress you intend to keep.'
Write-Host 'No Steam setting, game file or save was changed by Setup.'

$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Install-Common.ps1')
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
$launcher=Join-Path $PSScriptRoot 'Launch-Offline.ps1'
$pathFile=Join-Path $PSScriptRoot 'game-path.txt'
if(!(Test-Path -LiteralPath $pathFile)){Stop-AC8Problem 'SETUP_REQUIRED' '尚未设置游戏路径。' '请先运行 Setup.cmd。'}
$gameRoot=Resolve-AC8GameRoot (Get-Content -LiteralPath $pathFile -Raw -Encoding UTF8)
Assert-AC8PackageLocation $PSScriptRoot $gameRoot
if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'active-session.json')){
 & $launcher -CleanupOnly
}
Repair-AC8ResidualsInteractive $PSScriptRoot $gameRoot
& $launcher

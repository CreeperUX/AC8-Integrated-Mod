param([switch]$CheckOnly,[switch]$AllowNoSteam,[string]$GameRoot='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Install-Common.ps1')
$data=Get-AC8DataRoot $PSScriptRoot
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
$launcher=Join-Path $PSScriptRoot 'Launch-Offline.ps1'
if($GameRoot -and $GameRoot.Trim()){
 $gameRoot=Resolve-AC8GameRoot $GameRoot
}else{
 $pathFile=Join-Path $data 'game-path.txt'
 if(!(Test-Path -LiteralPath $pathFile)){Stop-AC8Problem 'SETUP_REQUIRED' '尚未设置游戏路径。' '请先运行 Setup.cmd。'}
 $gameRoot=Resolve-AC8GameRoot (Get-Content -LiteralPath $pathFile -Raw -Encoding UTF8)
}
Assert-AC8PackageLocation $PSScriptRoot $gameRoot
if($CheckOnly){& $launcher -CheckOnly -AllowNoSteam:$AllowNoSteam -GameRoot $gameRoot;return}
$operation=Enter-AC8Operation $gameRoot
try {
if(Test-Path -LiteralPath (Join-Path $data 'active-session.json')){
 & $launcher -CleanupOnly
}
Repair-AC8ResidualsInteractive $PSScriptRoot $gameRoot
& $launcher -AllowNoSteam:$AllowNoSteam -GameRoot $gameRoot

}finally{Exit-AC8Operation $operation}

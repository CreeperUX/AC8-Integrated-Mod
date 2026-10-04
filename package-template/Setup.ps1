$ErrorActionPreference='Stop'
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Close AC8 before setup.'}
$root=$PSScriptRoot
. (Join-Path $root 'Install-Common.ps1')
Write-Host 'AC8 安装向导：游戏根目录、Win64 目录和 AceCombat8.exe 路径均可。'
while($true){
 $inputPath=Read-Host '粘贴游戏路径（可带引号；输入 Q 取消）'
 if($inputPath.Trim() -ieq 'Q'){Stop-AC8Problem 'CANCELLED' '已取消安装。' '准备好游戏路径后可重新运行 Setup.cmd。'}
 try{$gameRoot=Resolve-AC8GameRoot $inputPath;break}catch{
  if($_.Exception.Data['AC8Code'] -ne 'GAME_PATH'){throw}
  Write-Host $_.Exception.Message -ForegroundColor Yellow
  Write-Host $_.Exception.Data['AC8Hint']
 }
}
Write-Host "已识别游戏根目录：$gameRoot"
Assert-AC8PackageLocation $root $gameRoot
$exe=Join-Path $gameRoot 'Game/Binaries/Win64/AceCombat8.exe'
if((Get-AC8FileHash -LiteralPath $exe).Hash -ne '51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'){
 Stop-AC8Problem 'GAME_BUILD' '游戏版本与本包适配版本不一致。' '本包适配 Build 25201480。请核对游戏版本与发布说明，不要替换游戏 EXE 绕过校验。'
}
$sessionPath=Join-Path $root 'active-session.json'
if(Test-Path -LiteralPath $sessionPath){
 $session=Get-Content -LiteralPath $sessionPath -Raw -Encoding UTF8 | ConvertFrom-Json
 if($session.Win64 -ne (Join-Path $gameRoot 'Game/Binaries/Win64')){Stop-AC8Problem 'OLD_SESSION' '此包还记录着另一游戏路径的未完成会话。' '先运行 Cleanup-Offline.cmd 完成原路径清理，再修改安装路径。'}
 & (Join-Path $root 'Launch-Offline.ps1') -CleanupOnly
}
Repair-AC8ResidualsInteractive $root $gameRoot
Assert-AC8WriteAccess $root
Assert-AC8WriteAccess (Join-Path $gameRoot 'Game/Binaries/Win64')
$candidates=@()
try{$steamPath=(Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath;if($steamPath){$candidates+=Join-Path $steamPath 'steam.exe'}}catch{}
try{$candidates+=@(Get-Process steam -ErrorAction Stop | ForEach-Object {$_.Path})}catch{}
if(${env:ProgramFiles(x86)}){$candidates+=Join-Path ${env:ProgramFiles(x86)} 'Steam/steam.exe'}
$steam=@($candidates | Where-Object {$_ -and (Test-Path -LiteralPath $_ -PathType Leaf)}) | Select-Object -First 1
if(!$steam){$steam=(Read-Host '未自动找到 Steam，请粘贴 steam.exe 的完整路径').Trim().Trim('"')}
if(!$steam -or !(Test-Path -LiteralPath $steam -PathType Leaf) -or [IO.Path]::GetFileName($steam) -ine 'steam.exe'){Stop-AC8Problem 'STEAM_PATH' '未找到有效的 steam.exe。' '请填写 Steam 程序文件路径，不是 steamapps 游戏库目录。'}
$steam=[IO.Path]::GetFullPath($steam)
& (Join-Path $root 'Choose-Features.ps1')
Set-Content -LiteralPath (Join-Path $root 'game-path.txt') -Value $gameRoot -Encoding UTF8
Set-Content -LiteralPath (Join-Path $root 'steam-path.txt') -Value $steam -Encoding UTF8
$option='"'+(Join-Path $root 'Start-AC8-From-Steam.cmd')+'" %command%'
Set-Content -LiteralPath (Join-Path $root 'Steam-Launch-Option.txt') -Value $option -Encoding UTF8
Write-Host '设置已保存。请将以下整行粘贴到 Steam → AC8 → 属性 → 通用 → 启动选项：'
Write-Host $option
Write-Host '然后从 Steam 或 Start.cmd 启动。发生云存档冲突时，请选择要保留的进度。'
Write-Host 'Steam 启动选项还需手动粘贴。普通游玩不需要 Python 或 NumPy。'

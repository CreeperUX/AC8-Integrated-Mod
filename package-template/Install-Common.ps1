. ([IO.Path]::Combine($PSScriptRoot,'PowerShell-Compat.ps1'))
function Stop-AC8Problem([string]$Code,[string]$Message,[string]$Hint) {
 $errorObject=New-Object InvalidOperationException($Message)
 $errorObject.Data['AC8Code']=$Code;$errorObject.Data['AC8Hint']=$Hint
 throw $errorObject
}
function Resolve-AC8GameRoot([string]$InputPath) {
 $value=$InputPath.Trim().Trim('"')
 if(!$value){Stop-AC8Problem 'GAME_PATH' '尚未输入游戏路径。' '可粘贴游戏根目录、Win64 目录或 AceCombat8.exe 的完整路径。'}
 try{$path=[IO.Path]::GetFullPath($value)}catch{Stop-AC8Problem 'GAME_PATH' '路径格式无效。' '请从资源管理器复制完整路径，不要输入 Steam 网页地址。'}
 if(Test-Path -LiteralPath $path -PathType Leaf){
  if([IO.Path]::GetFileName($path) -ine 'AceCombat8.exe'){Stop-AC8Problem 'GAME_PATH' '选择的文件不是 AceCombat8.exe。' '请重新选择游戏目录或 AceCombat8.exe。'}
  $path=Split-Path $path -Parent
 }
 for($level=0;$level -le 3;$level++){
  if(Test-Path -LiteralPath (Join-Path $path 'Game/Binaries/Win64/AceCombat8.exe') -PathType Leaf){return $path.TrimEnd('\','/')}
  $parent=Split-Path $path -Parent
  if(!$parent -or $parent -eq $path){break};$path=$parent
 }
 Stop-AC8Problem 'GAME_PATH' '未在所选位置找到 AC8 游戏。' 'Steam → 游戏 → 管理 → 浏览本地文件，复制打开的文件夹路径。输入 Q 可取消。'
}
function Assert-AC8PackageLocation([string]$PackageRoot,[string]$GameRoot) {
 $package=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\','/')
 $game=[IO.Path]::GetFullPath($GameRoot).TrimEnd('\','/')
 if($package.Equals($game,[StringComparison]::OrdinalIgnoreCase) -or $package.StartsWith($game+'\',[StringComparison]::OrdinalIgnoreCase)){
  Stop-AC8Problem 'PACKAGE_LOCATION' '整合包放在了游戏目录内部。' "请关闭此窗口，将整个整合包文件夹移到游戏目录之外（例如 D:\Mods\AC8-Integrated），再运行 Setup.cmd。当前整合包：$package"
 }
}
function Get-AC8Residuals([string]$GameRoot) {
 $w64=Join-Path $GameRoot 'Game/Binaries/Win64'
 return @('dwmapi.dll','ue4ss','steam_appid.txt' | Where-Object {Test-Path -LiteralPath (Join-Path $w64 $_)})
}
function Repair-AC8ResidualsInteractive([string]$PackageRoot,[string]$GameRoot) {
 $items=@(Get-AC8Residuals $GameRoot)
 if(!$items.Count){return}
 Write-Host ('检测到游戏目录已有加载器：'+($items -join '、')) -ForegroundColor Yellow
 Write-Host (Join-Path $GameRoot 'Game/Binaries/Win64')
 Write-Host '如果来自本整合包，可先备份校验再清理；若来自其他 Mod 或不清楚来源，请取消。'
 if((Read-Host '确认属于本整合包请输入 RECOVER；直接回车取消') -cne 'RECOVER'){
  Stop-AC8Problem 'LOADER_CONFLICT' '已保留现有加载器，未继续启动。' '确认文件归属后运行 Recover-Cleanup.cmd；不要直接删除未知 DLL。'
 }
 & (Join-Path $PackageRoot 'Recover-Cleanup.ps1') -GameRoot $GameRoot -RecoverHistorical
 if(@(Get-AC8Residuals $GameRoot).Count){Stop-AC8Problem 'LOADER_CONFLICT' '仍有加载器残留。' '请保留清理报告，并检查 Recover-Cleanup.cmd 的提示。'}
}
function Assert-AC8WriteAccess([string]$Directory) {
 $probe=Join-Path $Directory ('.ac8-write-test-'+[guid]::NewGuid().ToString('N')+'.tmp')
 try{
  $stream=[IO.File]::Open($probe,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
  $stream.Dispose()
 }catch{Stop-AC8Problem 'WRITE_ACCESS' "无法写入目录：$Directory" '请检查此目录的写入权限及 Windows 安全中心/安全软件的保护历史。不要关闭安全软件；修正具体权限或误拦截后重试。'}
 finally{if(Test-Path -LiteralPath $probe){Remove-Item -LiteralPath $probe -Force}}
}
function Show-AC8Problem($Failure,[string]$PackageRoot,[string]$Stage='操作') {
 if($Failure.Exception.Data['AC8Shown']){return}
 $Failure.Exception.Data['AC8Shown']=$true
 $hint=[string]$Failure.Exception.Data['AC8Hint']
 $message=$Failure.Exception.Message
 if(!$hint){
  if($Failure.CategoryInfo.Category -eq 'PermissionDenied' -or $Failure.Exception -is [UnauthorizedAccessException]){
   $hint='请检查报错文件的只读属性、目录权限及安全软件保护历史。通用写入检查通过，也不能保证 DLL 不被单独拦截。'
  }elseif($message -match 'marker|Unknown dwmapi|Other UE4SS|Filesystem link|Staged file changed'){
   $hint='清理器无法安全确认归属，已保留相关文件。请核对其他 Mod、目录链接或文件变化；详见 docs\CLEANUP.md。'
  }elseif($message -match 'Game is already running|Close AC8|Close the game'){
   $hint='请正常退出游戏，再重新运行。'
  }else{$hint='请保留下面的诊断文件，并按错误信息检查路径和文件。'}
 }
 Write-Host "[$Stage 未完成] $message" -ForegroundColor Red
 Write-Host $hint -ForegroundColor Yellow
 try{
  $folder=Join-Path $PackageRoot 'diagnostics';New-Item -ItemType Directory -Path $folder -Force | Out-Null
  $log=Join-Path $folder ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')+'.txt')
  @("Stage: $Stage",$Failure.ToString(),$Failure.ScriptStackTrace) | Set-Content -LiteralPath $log -Encoding UTF8
  Write-Host "诊断文件：$log"
 }catch{Write-Host '诊断文件无法保存，请保留当前窗口截图。'}
}
function Invoke-AC8OptionalAnalysis([string]$PackageRoot,[string]$Session) {
 $python=Get-Command python.exe -ErrorAction SilentlyContinue
 if(!$python -or $python.Source -match '[\\/]WindowsApps[\\/]'){
  Write-Host '已跳过可选分析：未找到可用 Python。普通游玩不需要安装 Python 或 NumPy。';return
 }
 # Collect native stderr locally; never let an optional analysis traceback replace a launch failure.
 $ErrorActionPreference='Continue'
 try{
  $probe=@(& $python.Source -c 'import numpy' 2>&1)
  if($LASTEXITCODE -ne 0){Write-Host '已跳过可选分析：当前 Python 缺少可用的 NumPy。原始数据已保留，不影响游戏和清理。';return}
  $log=Join-Path $Session 'analysis-output.txt'
  & $python.Source -X utf8 (Join-Path $PackageRoot 'tools/analyze_experiment.py') --session $Session *> $log
  if($LASTEXITCODE -ne 0){Write-Host "可选分析未完成，详细信息：$log"}else{Write-Host "分析报告已保存：$Session"}
 }catch{Write-Host '可选分析暂不可用，原始数据已保留。'}
}

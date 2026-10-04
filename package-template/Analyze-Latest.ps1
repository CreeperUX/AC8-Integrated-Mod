$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Install-Common.ps1')
$folder=Join-Path $PSScriptRoot 'sessions'
if(!(Test-Path -LiteralPath $folder)){Stop-AC8Problem 'NO_SESSION' '还没有可以分析的运行记录。' '先完成一次游戏运行，再使用此功能。'}
$session=Get-ChildItem -LiteralPath $folder -Directory | Sort-Object Name -Descending | Select-Object -First 1
if(!$session){Stop-AC8Problem 'NO_SESSION' '还没有可以分析的运行记录。' '先完成一次游戏运行，再使用此功能。'}
Invoke-AC8OptionalAnalysis $PSScriptRoot $session.FullName

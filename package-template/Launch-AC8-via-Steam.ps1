param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Install-Common.ps1')
$steam=([string](Get-Content -LiteralPath (Join-Path $PSScriptRoot 'steam-path.txt') -Raw -Encoding UTF8)).Trim()
if(!$steam){Stop-AC8Problem 'SETUP_REQUIRED' '尚未完成安装设置。' '请先运行 Setup.cmd，再从 Start.cmd 启动。'}
$bridge=Join-Path $PSScriptRoot 'Start-AC8-From-Steam.cmd'
$expected='"'+$bridge+'" %command%'
if(!(Test-Path -LiteralPath $steam)){Stop-AC8Problem 'STEAM_PATH' '之前保存的 Steam 路径已失效。' '请重新运行 Setup.cmd 更新路径。'}
if(!(Test-Path -LiteralPath $bridge)){throw 'Steam mod bridge not found.'}
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
# Read only the app-specific launch setting, never print account/auth data.
$configured=$false
$userRoots=Join-Path (Split-Path $steam) 'userdata'
$users=@(Get-ChildItem -LiteralPath $userRoots -Directory -ErrorAction SilentlyContinue | Where-Object {$_.Name -match '^\d+$' -and $_.Name -ne '0'})
$activeUser=$null
try{$value=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam\ActiveProcess' -ErrorAction Stop).ActiveUser;$accountId=([long]$value -band 4294967295);if($accountId -gt 0){$activeUser=[string]$accountId}}catch{}
if($activeUser){$users=@($users | Where-Object Name -eq $activeUser)}
elseif($users.Count -gt 1){Stop-AC8Problem 'STEAM_ACCOUNT' 'Cannot verify the active Steam account launch option.' 'Open Steam with the intended account and verify its AC8 launch option; then retry.'}
foreach($user in $users){
 $file=Join-Path $user.FullName 'config/localconfig.vdf'
 if(!(Test-Path -LiteralPath $file)){continue}
 $text=Get-Content -LiteralPath $file -Raw -Encoding UTF8
 $match=[regex]::Match($text,'"2288340"\s*\{')
 if(!$match.Success){continue}
 $start=$match.Index+$match.Length;$i=$start;$depth=1;$quoted=$false;$escape=$false
 while($i -lt $text.Length -and $depth -gt 0){
  $c=$text[$i]
  if($escape){$escape=$false}
  elseif($quoted -and $c -eq '\'){$escape=$true}
  elseif($c -eq '"'){$quoted=!$quoted}
  elseif(!$quoted -and $c -eq '{'){$depth++}
  elseif(!$quoted -and $c -eq '}'){$depth--}
  $i++
 }
 if($depth){continue}
 $body=$text.Substring($start,$i-$start-1)
 $option=[regex]::Match($body,'"LaunchOptions"\s*"((?:\\.|[^"\\])*)"')
 if($option.Success){
  $value=[regex]::Replace($option.Groups[1].Value,'\\([\\"])','$1')
  if($value.Trim() -eq $expected){$configured=$true}
 }
}
if(!$configured){
 Write-Host '请将以下整行粘贴到 Steam → AC8 → 属性 → 通用 → 启动选项：'
 Write-Host $expected
 Write-Host '刚设置后 Steam 可能尚未保存配置，请直接在 Steam 点击一次开始游戏。'
 if($CheckOnly){exit 2}
 Stop-AC8Problem 'STEAM_OPTIONS' '尚未检测到本包的 Steam 启动选项。' '完成上面的设置，或在 Steam 中点击开始游戏。'
}
Write-Host 'Steam launch option verified. Steam will handle its own Cloud pre-launch checks.'
if($CheckOnly){exit 0}
# Do not stage mods or launch the game here. Steam first performs cloud checks,
# then invokes Start-AC8-From-Steam.cmd, which manages the complete game session.
Start-Process -FilePath $steam -ArgumentList '-applaunch','2288340' -WindowStyle Hidden

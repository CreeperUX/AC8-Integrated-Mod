param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
$steam=([string](Get-Content -LiteralPath (Join-Path $PSScriptRoot 'steam-path.txt') -Raw)).Trim()
if(!$steam){throw 'Run Setup.cmd first.'}
$bridge=Join-Path $PSScriptRoot 'Start-AC8-From-Steam.cmd'
$expected='"'+$bridge+'" %command%'
if(!(Test-Path -LiteralPath $steam)){throw 'Steam executable not found.'}
if(!(Test-Path -LiteralPath $bridge)){throw 'Steam mod bridge not found.'}
if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw 'Game is already running.'}
# Read only the app-specific launch setting, never print account/auth data.
$configured=$false
$userRoots=Join-Path (Split-Path $steam) 'userdata'
foreach($user in Get-ChildItem -LiteralPath $userRoots -Directory){
 $file=Join-Path $user.FullName 'config/localconfig.vdf'
 if(!(Test-Path -LiteralPath $file)){continue}
 $text=Get-Content -LiteralPath $file -Raw
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
 Write-Host 'One-time setup needed: Steam > AC8 > Properties > General > Launch Options'
 Write-Host $expected
 Write-Host 'If you just set this and Steam has not saved it yet, launch directly with Steam Play once.'
 if($CheckOnly){exit 2}
 throw 'Steam mod launch option is not yet recorded. No game started; no files changed.'
}
Write-Host 'Steam launch option verified. Steam will handle its own Cloud pre-launch checks.'
if($CheckOnly){exit 0}
# Do not stage mods or launch the game here. Steam first performs cloud checks,
# then invokes Start-AC8-From-Steam.cmd, which manages the complete game session.
Start-Process -FilePath $steam -ArgumentList '-applaunch','2288340' -WindowStyle Hidden

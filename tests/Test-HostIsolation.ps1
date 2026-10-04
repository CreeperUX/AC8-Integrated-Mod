$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$template=Join-Path $repo 'package-template'
. (Join-Path $template 'PowerShell-Compat.ps1')
. (Join-Path $template 'Cleanup-Core.ps1')
$root=Join-Path $repo ('native/test-runtime/host-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$vector=Join-Path $root 'literal [abc].txt'
[IO.File]::WriteAllText($vector,'abc',[Text.UTF8Encoding]::new($false))
if((Get-AC8FileHash -LiteralPath $vector).Hash -ne 'BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD'){throw 'SHA256 vector failed'}
$stream=[IO.File]::Open($vector,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try{$caught=$false;try{$null=Get-AC8FileHash $vector}catch{$caught=$true};if(!$caught){throw 'Locked file unexpectedly read'}}finally{$stream.Dispose()}
$null=Get-AC8FileHash $vector
foreach($file in Get-ChildItem -LiteralPath $template -File -Recurse -Filter '*.ps1'){
 $tokens=$null;$parseErrors=$null
 $ast=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$parseErrors)
 if($parseErrors.Count){throw $parseErrors[0]}
 $calls=$ast.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -match '(^|\\)Get-FileHash$'},$true)
 if($calls.Count){throw "Direct Get-FileHash dependency: $($file.Name)"}
}
Write-Host 'PASS SHA256 standard vector, literal brackets and stream release after errors'
$package=Join-Path $root 'package';Copy-Item -LiteralPath $template -Destination $package -Recurse
foreach($name in 'tools','models'){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $package -Recurse}
Set-Content -LiteralPath (Join-Path $package 'package-info.json') -Value '{}'
$payload=Join-Path $package 'payload'
$manifest=@(Get-ChildItem -LiteralPath $payload -Recurse -File | ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($payload.Length+1);SHA256=(Get-AC8FileHash $_.FullName).Hash}})
ConvertTo-Json -InputObject $manifest -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'payload-manifest.json')
$game=Join-Path $root 'game';$w64=Join-Path $game 'Game/Binaries/Win64'
New-Item -ItemType Directory -Path $w64 -Force | Out-Null
Set-Content -LiteralPath (Join-Path $w64 'AceCombat8.exe') -Value 'fixture only'
$steam=Join-Path $root 'steam.exe';Set-Content -LiteralPath $steam -Value 'fixture only'
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value $game -Encoding UTF8
Set-Content -LiteralPath (Join-Path $package 'steam-path.txt') -Value $steam -Encoding UTF8
$worker=Join-Path $root 'isolated.ps1'
@'
param($Package,$Game,$Steam,$Action)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
Import-Module Microsoft.PowerShell.Management
Import-Module Microsoft.PowerShell.Utility
if(Test-Path Function:\Get-FileHash){Remove-Item Function:\Get-FileHash -ErrorAction Stop}
$PSModuleAutoLoadingPreference='None';$env:PSModulePath=''
if(Get-Command Get-FileHash -ErrorAction SilentlyContinue){throw 'Missing-command fixture failed'}
. ([IO.Path]::Combine($Package,'PowerShell-Compat.ps1'))
$global:AC8FixtureRealHash=(Get-Command Get-AC8FileHash).ScriptBlock
function Get-AC8FileHash {param($LiteralPath)
 if([IO.Path]::GetFileName($LiteralPath) -eq 'AceCombat8.exe'){return [pscustomobject]@{Hash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'}}
 & $global:AC8FixtureRealHash -LiteralPath $LiteralPath
}
function Get-Process {param($Name,$ErrorAction) if($Name -eq 'steam'){return [pscustomobject]@{Id=1}}}
function Start-Process {throw 'Preflight attempted to start a process'}
function Read-Host {throw 'Preflight unexpectedly prompted'}
& (Join-Path $Package 'Run-Console.ps1') -Action $Action -CheckOnly
exit $LASTEXITCODE
'@ | Set-Content -LiteralPath $worker -Encoding UTF8
$hostExe=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$before=@(Get-AC8Snapshot $package) | ConvertTo-Json -Depth 4 -Compress
& $hostExe -NoProfile -ExecutionPolicy Bypass -File $worker $package $game $steam Session
if($LASTEXITCODE -ne 0){throw 'Isolated full preflight failed'}
if($before -ne (@(Get-AC8Snapshot $package) | ConvertTo-Json -Depth 4 -Compress)){throw 'Preflight changed package'}
if(@(Get-ChildItem -LiteralPath $w64 -Force).Count -ne 1){throw 'Preflight staged files'}
Write-Host 'PASS missing hash command, empty module path, auto-load disabled through full Session/CheckOnly chain'
# Existing state must be reported, never silently cleaned during CheckOnly.
Set-Content -LiteralPath (Join-Path $package 'active-session.json') -Value '{"Id":"fixture"}'
$before=@(Get-AC8Snapshot $package) | ConvertTo-Json -Depth 4 -Compress
& $hostExe -NoProfile -ExecutionPolicy Bypass -File $worker $package $game $steam Session
if($LASTEXITCODE -eq 0){throw 'Stale state preflight incorrectly passed'}
if($before -ne (@(Get-AC8Snapshot $package) | ConvertTo-Json -Depth 4 -Compress)){throw 'Read-only check modified stale state'}
& $hostExe -NoProfile -ExecutionPolicy Bypass -File $worker $package $game $steam Cleanup
if($LASTEXITCODE -eq 0){throw 'Cleanup plus CheckOnly was accepted by console'}
# A linked game directory must fail before deployment, because cleanup refuses links.
$linkedGame=Join-Path $root 'linked-game';New-Item -ItemType Junction -Path $linkedGame -Target $game | Out-Null
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value $linkedGame -Encoding UTF8
& $hostExe -NoProfile -ExecutionPolicy Bypass -File $worker $package $linkedGame $steam Session
if($LASTEXITCODE -eq 0 -or @(Get-ChildItem -LiteralPath $w64 -Force).Count -ne 1){throw 'Linked game was staged or incorrectly accepted'}
Set-Content -LiteralPath (Join-Path $package 'game-path.txt') -Value $game -Encoding UTF8
Write-Host 'PASS stale-session read-only failure and invalid CheckOnly action refusal'
Remove-Item -LiteralPath (Join-Path $package 'active-session.json')
# The GUI background pipeline starts in its own session state.
$ps=[PowerShell]::Create()
try{
 [void]$ps.AddScript({param($Package,$Game,$Steam)
  $oldPath=$env:PSModulePath
  try{
   Import-Module Microsoft.PowerShell.Management;Import-Module Microsoft.PowerShell.Utility
   if(Test-Path Function:\Get-FileHash){Remove-Item Function:\Get-FileHash -ErrorAction Stop}
   $PSModuleAutoLoadingPreference='None';$env:PSModulePath=''
   . ([IO.Path]::Combine($Package,'PowerShell-Compat.ps1'))
   $global:AC8FixtureRealHash=(Get-Command Get-AC8FileHash).ScriptBlock
   # Replace only the fake EXE's expected hash; all package files use real SHA256.
   function Get-AC8FileHash {param($LiteralPath)
    if([IO.Path]::GetFileName($LiteralPath) -eq 'AceCombat8.exe'){return [pscustomobject]@{Hash='51510E2A520565DBE81FB0D569E95CD4393077ACAAA859371489B80B8128829F'}}
    & $global:AC8FixtureRealHash -LiteralPath $LiteralPath
   }
   function Get-Process {param($Name,$ErrorAction) return $null}
   & (Join-Path $Package 'Gui-Worker.ps1') -Action Check -Root $Package -GamePath $Game -SteamPath $Steam -MissileMode guidance
  }finally{$env:PSModulePath=$oldPath}
 }).AddArgument($package).AddArgument($game).AddArgument($steam)
 $result=@($ps.Invoke());if($ps.HadErrors -or !$result[-1].Success){throw ('Isolated GUI check failed: '+($ps.Streams.Error -join ';')+' result='+($result[-1] | ConvertTo-Json -Compress))}
}finally{$ps.Dispose()}
Write-Host 'PASS actual GUI worker in isolated runspace with missing Get-FileHash and empty module path'
$lock=Enter-AC8Operation $game
$other=[PowerShell]::Create()
try{
 [void]$other.AddScript({param($Compat,$Game)
  . $Compat
  try{$held=Enter-AC8Operation $Game;Exit-AC8Operation $held;return 'unexpected'}catch{return $_.Exception.Data['AC8Code']}
 }).AddArgument((Join-Path $template 'PowerShell-Compat.ps1')).AddArgument($game)
 $pending=$other.BeginInvoke();$busy=@($other.EndInvoke($pending));if($busy.Count -ne 1 -or $busy[-1] -ne 'SESSION_BUSY'){throw ('Concurrent operation was not refused: '+($busy -join ',')+' errors='+($other.Streams.Error -join ';'))}
 # Same-thread nesting is used by launcher -> cleanup core.
 $nested=Enter-AC8Operation $game;Exit-AC8Operation $nested
}finally{$other.Dispose();Exit-AC8Operation $lock}
$released=Enter-AC8Operation $game;Exit-AC8Operation $released
Write-Host 'PASS cross-thread exclusion, same-thread nested cleanup and lock release'
$global:LASTEXITCODE=0

# Read only Steam registration and its library/app manifests. No disk-wide scan.
function Read-AC8ValveData([string]$Path){
 if((Get-Item -LiteralPath $Path).Length -gt 2097152){throw 'Steam manifest too large.'}
 $raw=Get-Content -LiteralPath $Path -Raw -Encoding UTF8
 $matches=[regex]::Matches($raw,'"(?:\\.|[^"\\])*"|//[^\r\n]*|[{}]|[^\s{}"]+')
 $tokens=@($matches | Where-Object {$_.Value -notmatch '^//'} | ForEach-Object {$_.Value})
 $cursor=[pscustomobject]@{Index=0}
 function Read-Value([string]$Token){
  if($Token.StartsWith('"')){return [regex]::Replace($Token.Substring(1,$Token.Length-2),'\\([\\"])','$1')}
  return $Token
 }
 function Read-Object([bool]$Nested,[int]$Depth=0){
  if($Depth -gt 24){throw 'Steam manifest nesting too deep.'}
  $result=@{}
  while($cursor.Index -lt $tokens.Count){
   $token=$tokens[$cursor.Index];$cursor.Index++
   if($token -eq '}'){if(!$Nested){throw 'Unexpected closing brace.'};return $result}
   if($token -eq '{' -or $cursor.Index -ge $tokens.Count){throw 'Invalid Steam manifest.'}
   $key=Read-Value $token;$value=$tokens[$cursor.Index];$cursor.Index++
   if($result.ContainsKey($key)){throw 'Duplicate Steam manifest key.'}
   if($value -eq '{'){$result[$key]=Read-Object $true ($Depth+1)}
   elseif($value -eq '}'){throw 'Missing Steam manifest value.'}
   else{$result[$key]=Read-Value $value}
  }
  if($Nested){throw 'Unclosed Steam manifest object.'}
  return $result
 }
 return Read-Object $false
}
function Get-AC8SteamCandidates([string]$Preferred){
 $paths=@($Preferred)
 foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam','HKLM:\SOFTWARE\Valve\Steam')){
  try{
   $entry=Get-ItemProperty -LiteralPath $key -ErrorAction Stop
   if($entry.SteamExe){$paths+=$entry.SteamExe}
   foreach($value in @($entry.SteamPath,$entry.InstallPath)){if($value){$paths+=Join-Path $value 'steam.exe'}}
  }catch{}
 }
 try{$paths+=@(Get-Process steam -ErrorAction Stop | ForEach-Object {$_.Path})}catch{}
 foreach($base in @(${env:ProgramFiles(x86)},$env:ProgramFiles)){if($base){$paths+=Join-Path $base 'Steam/steam.exe'}}
 $found=@()
 foreach($path in $paths){
  try{
   if($path -and [IO.Path]::GetFileName($path) -ieq 'steam.exe' -and (Test-Path -LiteralPath $path -PathType Leaf)){
    $full=[IO.Path]::GetFullPath($path)
    if($found -notcontains $full){$found+=$full}
   }
  }catch{}
 }
 return $found
}
function Find-AC8Installations {
 param([string]$GamePath,[string]$SteamPath)
 $gameInput=$GamePath.Trim().Trim('"');$steamInput=$SteamPath.Trim().Trim('"')
 $steams=@(Get-AC8SteamCandidates $steamInput)
 $selectedSteam=$steamInput
 if(!$selectedSteam -and $steams.Count -eq 1){$selectedSteam=$steams[0]}
 $scan=@($steams)
 if($selectedSteam -and $steams -contains $selectedSteam){$scan=@($selectedSteam)}
 $games=@();$warnings=@()
 foreach($steam in $scan){
  $steamRoot=Split-Path $steam -Parent
  $libraries=@($steamRoot)
  foreach($manifest in @((Join-Path $steamRoot 'steamapps/libraryfolders.vdf'),(Join-Path $steamRoot 'config/libraryfolders.vdf'))){
   if(!(Test-Path -LiteralPath $manifest -PathType Leaf)){continue}
   try{
    $data=Read-AC8ValveData $manifest
    foreach($key in @($data.libraryfolders.Keys)){
     if($key -notmatch '^\d+$'){continue}
     $item=$data.libraryfolders[$key]
     $library=if($item -is [System.Collections.IDictionary]){$item.path}else{$item}
     if($library -and [IO.Path]::IsPathRooted($library) -and $libraries -notcontains $library){$libraries+=$library}
    }
   }catch{$warnings+='部分 Steam 库清单无法读取，可手动选择游戏位置。'}
  }
  foreach($library in $libraries){
   $manifest=Join-Path $library 'steamapps/appmanifest_2288340.acf'
   if(!(Test-Path -LiteralPath $manifest -PathType Leaf)){continue}
   try{
    $data=Read-AC8ValveData $manifest
    $app=$data.AppState
    if($app.appid -ne '2288340' -or !$app.installdir -or [IO.Path]::IsPathRooted($app.installdir)){continue}
    $common=[IO.Path]::GetFullPath((Join-Path $library 'steamapps/common')).TrimEnd('\')
    $candidate=[IO.Path]::GetFullPath((Join-Path $common $app.installdir))
    if(!$candidate.StartsWith($common+'\',[StringComparison]::OrdinalIgnoreCase)){continue}
    if((Test-Path -LiteralPath (Join-Path $candidate 'Game/Binaries/Win64/AceCombat8.exe') -PathType Leaf) -and $games -notcontains $candidate){$games+=$candidate}
   }catch{$warnings+='部分游戏清单无法读取，可手动选择游戏位置。'}
  }
 }
 $selectedGame=$gameInput
 if(!$selectedGame -and $games.Count -eq 1){$selectedGame=$games[0]}
 return [pscustomobject]@{GamePath=$selectedGame;SteamPath=$selectedSteam;GameCandidates=@($games);SteamCandidates=@($steams);Warnings=@($warnings | Select-Object -Unique)}
}

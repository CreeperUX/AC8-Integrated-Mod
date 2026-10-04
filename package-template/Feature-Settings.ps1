function Read-FeatureSettings([string]$Path) {
 $mode='full';$seen=$false
 if(Test-Path -LiteralPath $Path){
  foreach($line in Get-Content -LiteralPath $Path){
   $clean=$line.Trim();if(!$clean -or $clean.StartsWith(';')){continue}
   if($seen){throw 'Duplicate or mixed feature settings. Keep one missile_mode entry.'}
   if($clean -match '^missile_mode\s*=\s*(guidance|full|none)$'){$mode=$Matches[1].ToLowerInvariant()}
   elseif($clean -match '^missile_enhancement\s*=\s*([01])$'){$mode=if($Matches[1] -eq '1'){'full'}else{'none'}}
   else{throw "Invalid feature setting: $clean"}
   $seen=$true
  }
 }
 $count=switch($mode){'guidance'{32} 'full'{375} 'none'{0}}
 return [pscustomobject]@{MissileMode=$mode;MissileEnhancement=($mode -ne 'none');ExpectedFields=$count}
}
function Install-SelectedUE4SS([string]$PayloadRoot,[string]$Destination,$Features) {
 $source=[IO.Path]::GetFullPath($PayloadRoot);$target=[IO.Path]::GetFullPath($Destination)
 if($source.Equals($target,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload and staging directory must differ.'}
 if($Features.MissileMode -notin @('none','guidance','full')){throw 'Invalid missile mode.'}
 if(@(Get-ChildItem -LiteralPath $target -Force | Where-Object Name -ne 'AC8SourceInit-owner.txt').Count){throw 'Staging directory is not empty; finish owned cleanup first.'}
 foreach($file in Get-ChildItem -LiteralPath $source -Force){
  if($file.Name -eq 'Mods'){continue}
  Copy-Item -LiteralPath $file.FullName -Destination $target -Recurse
 }
 $mods=Join-Path $target 'Mods';New-Item -ItemType Directory -Path $mods -Force | Out-Null
 Copy-Item -LiteralPath (Join-Path $source 'Mods/AC8MouseAim') -Destination $mods -Recurse
 if($Features.MissileEnhancement){
  Copy-Item -LiteralPath (Join-Path $source 'Mods/AC8SourceInit') -Destination $mods -Recurse
  Set-Content -LiteralPath (Join-Path $mods 'AC8SourceInit/Scripts/installation_mode.lua') -Value ("return '"+$Features.MissileMode+"'") -Encoding ASCII
 }
 $lines=@('AC8MouseAim : 1')
 if($Features.MissileEnhancement){$lines=@('AC8SourceInit : 1')+$lines}
 Set-Content -LiteralPath (Join-Path $mods 'mods.txt') -Value $lines -Encoding ASCII
}
function Get-FeatureReadinessPattern($Features) {
 if($Features.MissileEnhancement){return '\[AC8SourceInit\].*SOURCE_APPLIED mode='+$Features.MissileMode+'\b.*verified='+$Features.ExpectedFields+'\b'}
 return '\[AC8MouseAim\].*Loaded\.'
}

function Read-FeatureSettings([string]$Path) {
 $enabled=$true
 if(Test-Path -LiteralPath $Path){
  foreach($line in Get-Content -LiteralPath $Path){
   $clean=$line.Trim();if(!$clean -or $clean.StartsWith(';')){continue}
   if($clean -notmatch '^missile_enhancement\s*=\s*([01])$'){throw "Invalid feature setting: $clean"}
   $enabled=$Matches[1] -eq '1'
  }
 }
 return [pscustomobject]@{MissileEnhancement=$enabled}
}
function Install-SelectedUE4SS([string]$PayloadRoot,[string]$Destination,$Features) {
 $source=[IO.Path]::GetFullPath($PayloadRoot);$target=[IO.Path]::GetFullPath($Destination)
 if($source.Equals($target,[StringComparison]::OrdinalIgnoreCase)){throw 'Payload and staging directory must differ.'}
 if($Features.MissileEnhancement -isnot [bool]){throw 'Feature selection must be a boolean.'}
 if(@(Get-ChildItem -LiteralPath $target -Force | Where-Object Name -ne 'AC8SourceInit-owner.txt').Count){throw 'Staging directory is not empty; finish owned cleanup first.'}
 foreach($file in Get-ChildItem -LiteralPath $source -Force){
  if($file.Name -eq 'Mods'){continue}
  Copy-Item -LiteralPath $file.FullName -Destination $target -Recurse
 }
 $mods=Join-Path $target 'Mods';New-Item -ItemType Directory -Path $mods -Force | Out-Null
 Copy-Item -LiteralPath (Join-Path $source 'Mods/AC8MouseAim') -Destination $mods -Recurse
 if($Features.MissileEnhancement){Copy-Item -LiteralPath (Join-Path $source 'Mods/AC8SourceInit') -Destination $mods -Recurse}
 $lines=@('AC8MouseAim : 1')
 if($Features.MissileEnhancement){$lines=@('AC8SourceInit : 1')+$lines}
 Set-Content -LiteralPath (Join-Path $mods 'mods.txt') -Value $lines -Encoding ASCII
}
function Get-FeatureReadinessPattern($Features) {
 if($Features.MissileEnhancement){return '\[AC8SourceInit\].*SOURCE_APPLIED.*verified=375'}
 return '\[AC8MouseAim\].*Loaded\.'
}

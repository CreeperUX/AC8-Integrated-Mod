$ErrorActionPreference='Stop'
# MouseAim-Settings.ini -> runtime config.ini: hud_boresight (2.4.1) is read, range-checked and written into [control],
# also into a runtime config.ini from 2.4.0 that has no hud_boresight line yet.
$repo=Split-Path $PSScriptRoot -Parent;$template=Join-Path $repo 'package-template'
. (Join-Path $template 'MouseAim-Settings.ps1')
$fixture=Join-Path $repo ('native/test-runtime/mouse-settings-'+[guid]::NewGuid().ToString('N'))
$config=Get-Content -LiteralPath (Join-Path $template 'payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/config.ini') -Raw
$settings=Get-Content -LiteralPath (Join-Path $template 'MouseAim-Settings.ini') -Raw
if((Read-MouseSettings (Join-Path $template 'MouseAim-Settings.ini')).hud_boresight -ne 1){throw 'Default gun cross must be shown'}
$legacy=$config -replace '(?m)^; Reference gun cross[^\r\n]*\r?\n','' -replace '(?m)^hud_boresight=[^\r\n]*\r?\n',''
if($legacy -match 'hud_boresight'){throw 'Fixture still has hud_boresight'}
foreach($case in @(@{name='current';text=$config},@{name='legacy';text=$legacy})){
 $mod=Join-Path $fixture $case.name;New-Item -ItemType Directory -Path (Join-Path $mod 'Scripts') -Force | Out-Null
 Set-Content -LiteralPath (Join-Path $mod 'config.ini') -Value $case.text -NoNewline -Encoding ASCII
 foreach($want in 0,1){
  $ini=Join-Path $fixture "user-$want.ini";Set-Content -LiteralPath $ini -Value ($settings -replace '(?m)^hud_boresight=1',"hud_boresight=$want") -Encoding UTF8
  Apply-MouseSettings (Read-MouseSettings $ini) $mod
  $text=Get-Content -LiteralPath (Join-Path $mod 'config.ini') -Raw;$lines=[regex]::Matches($text,'(?m)^hud_boresight=([^\r\n]*)')
  if($lines.Count -ne 1 -or $lines[0].Groups[1].Value -ne "$want"){throw "$($case.name): hud_boresight=$want not written exactly once"}
  if($text.IndexOf('hud_boresight=') -gt $text.IndexOf('[keybindings]')){throw "$($case.name): hud_boresight outside [control]"}
 }
}
foreach($bad in '2','0.5'){
 $ini=Join-Path $fixture 'bad.ini';Set-Content -LiteralPath $ini -Value ($settings -replace '(?m)^hud_boresight=1',"hud_boresight=$bad") -Encoding UTF8
 $caught=$false;try{Read-MouseSettings $ini|Out-Null}catch{$caught=$true};if(!$caught){throw "hud_boresight=$bad accepted"}
}
Remove-Item -LiteralPath $fixture -Recurse -Force
Write-Host 'PASS hud_boresight default, 0/1 written once into [control] (also for a 2.4.0 runtime config without the line), invalid values rejected'

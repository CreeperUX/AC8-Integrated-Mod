function Get-AC8KeybindingSpec {
 return Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Keybindings.schema.json') -Raw -Encoding UTF8 | ConvertFrom-Json
}
function Get-AC8DefaultBindings {
 $values=[ordered]@{};foreach($action in (Get-AC8KeybindingSpec).actions){$values[$action.name]=[string]$action.default};return $values
}
function Convert-AC8KeyToken([string]$Token) {
 $matches=@((Get-AC8KeybindingSpec).keys.PSObject.Properties | Where-Object Name -ieq $Token.Trim())
 if($matches.Count -ne 1){throw "不支持的按键：$Token。F1–F10为已有功能保留；不接受组合键。"}
 return [string]$matches[0].Name
}
function Assert-AC8Keybindings($Values) {
 $spec=Get-AC8KeybindingSpec;$seen=@{};$known=@($spec.actions | ForEach-Object name)
 foreach($key in $Values.Keys){if($key -notin $known){throw "未知键位功能：$key"}}
 foreach($action in $spec.actions){
  if(!$Values.Contains($action.name)){throw "缺少键位功能：$($action.name)"}
  $token=Convert-AC8KeyToken ([string]$Values[$action.name]);$vk=[int]$spec.keys.PSObject.Properties[$token].Value.vk
  if($vk -ne 0 -and $seen.ContainsKey($vk)){throw "按键冲突：$token 同时用于[$($seen[$vk])]和[$($action.label)]。请更换其中一项。"}
  if($vk -ne 0){$seen[$vk]=[string]$action.label}
 }
}
function Read-AC8Keybindings([string]$Path) {
 $values=Get-AC8DefaultBindings;$seen=@{};$section=$false
 if(Test-Path -LiteralPath $Path){foreach($line in (Get-Content -LiteralPath $Path -Encoding UTF8)){
  $text=$line.Trim();if(!$text -or $text.StartsWith(';') -or $text.StartsWith('#')){continue}
  if($text -ieq '[bindings]'){if($section){throw '重复的 bindings 分区。'};$section=$true;continue}
  if(!$section -or $text -notmatch '^([a-z_]+)\s*=\s*([A-Za-z0-9]+)$'){throw "无效键位配置：$text"}
  $key=$Matches[1];$token=$Matches[2]
  if(!$values.Contains($key)){throw "未知键位功能：$key"};if($seen.ContainsKey($key)){throw "重复键位功能：$key"}
  $seen[$key]=$true;$values[$key]=Convert-AC8KeyToken $token
 }}
 Assert-AC8Keybindings $values;return $values
}
function Save-AC8Keybindings([string]$Root,$Values) {
 if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw '请先退出游戏再保存键位；新配置在下次启动生效。'}
 Assert-AC8Keybindings $Values
 $path=Join-Path $Root 'Keybindings.ini';$temporary=$path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
 $lines=@('; Saved by AC8 GUI. Applied on next launch.','; Manual axes must match the game bindings.','[bindings]')
 foreach($action in (Get-AC8KeybindingSpec).actions){$lines+=($action.name+'='+(Convert-AC8KeyToken ([string]$Values[$action.name])))}
 try {
  [IO.File]::WriteAllText($temporary,($lines -join [Environment]::NewLine)+[Environment]::NewLine,[Text.Encoding]::ASCII)
  if(Get-Process AceCombat8 -ErrorAction SilentlyContinue){throw '游戏正在运行，键位未保存。'}
  if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination ($path+'.bak') -Force}
  Move-Item -LiteralPath $temporary -Destination $path -Force
 }finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
}
function Apply-AC8Keybindings($Values,[string]$ModPath) {
 Assert-AC8Keybindings $Values;$spec=Get-AC8KeybindingSpec
 $path=Join-Path $ModPath 'config.ini';$text=Get-Content -LiteralPath $path -Raw
 $text=[regex]::Replace($text,'(?ms)^\[keybindings\]\s*\r?\n.*?(?=^\[|\z)','')
 $lines=@('[keybindings]');foreach($action in $spec.actions){$token=Convert-AC8KeyToken ([string]$Values[$action.name]);$vk=[int]$spec.keys.PSObject.Properties[$token].Value.vk;$lines+=($action.name+'='+$vk)}
 [IO.File]::WriteAllText($path,$text.TrimEnd()+[Environment]::NewLine+[Environment]::NewLine+($lines -join [Environment]::NewLine)+[Environment]::NewLine,[Text.Encoding]::ASCII)
}

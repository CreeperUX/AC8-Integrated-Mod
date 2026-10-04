function Read-MouseSettings([string]$Path) {
 $values=@{sensitivity=0.10;hud_renderer=1;hud_connector=1;hud_link_always=0;hud_opacity=0.65;control_mode=1;model_assist=1;model_assist_strength=0.20;mouse_reference_fov=62;arrival_braking=1.15;hud_fps=120;follow_rate=8;level_rate=3;camera_mode=1;camera_distance_m=36;camera_height_m=6}
 if(Test-Path -LiteralPath $Path){
  foreach($line in (Get-Content -LiteralPath $Path)){
   $clean=$line.Trim();if(!$clean -or $clean.StartsWith(';')){continue}
   if($clean -notmatch '^([a-z_]+)\s*=\s*([0-9]+(?:\.[0-9]+)?)$'){throw "Invalid mouse setting: $clean"}
   $key=$Matches[1];if(!$values.ContainsKey($key)){throw "Unknown mouse setting: $key"}
   $values[$key]=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
  }
 }
 foreach($rule in @(@('hud_renderer',0,1),@('hud_connector',0,1),@('hud_link_always',0,1),@('hud_opacity',0.15,1),@('model_assist',0,1),@('control_mode',0,1),@('model_assist_strength',0,0.35),@('mouse_reference_fov',30,150),@('arrival_braking',1,1.35),@('sensitivity',0.01,1),@('hud_fps',60,240),@('follow_rate',2,20),@('level_rate',0,10),@('camera_mode',0,1),@('camera_distance_m',10,100),@('camera_height_m',0,20))){
  $value=$values[$rule[0]];if([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt $rule[1] -or $value -gt $rule[2]){throw "Mouse setting out of range: $($rule[0])"}
 }
 foreach($key in @('hud_renderer','hud_connector','hud_link_always','control_mode','camera_mode')){if($values[$key] -ne 0 -and $values[$key] -ne 1){throw "$key must be 0 or 1"}}
 if($values.model_assist -ne 0 -and $values.model_assist -ne 1){throw 'model_assist must be 0 or 1'}
 if($values.hud_fps -ne [math]::Floor($values.hud_fps)){throw 'hud_fps must be an integer'}
 return $values
}
function Apply-MouseSettings($Values,[string]$ModPath) {
 $fmt=[Globalization.CultureInfo]::InvariantCulture
 $cfg=Join-Path $ModPath 'config.ini'
 $text=Get-Content -LiteralPath $cfg -Raw
 foreach($name in @('sensitivity','hud_fps','mouse_reference_fov','arrival_braking','control_mode','model_assist','model_assist_strength','hud_renderer','hud_connector','hud_link_always','hud_opacity','camera_mode','camera_distance_m','camera_height_m')){
  $value=$Values[$name].ToString($fmt)
  $text=[regex]::Replace($text,"(?m)^$name=.*$","$name=$value")
 }
 Set-Content -LiteralPath $cfg -Value $text -Encoding ASCII
 $follow=$Values.follow_rate.ToString($fmt);$level=$Values.level_rate.ToString($fmt)
 Set-Content -LiteralPath (Join-Path $ModPath 'Scripts/camera-settings.lua') -Value "return {follow_rate=$follow,level_rate=$level}" -Encoding ASCII
}

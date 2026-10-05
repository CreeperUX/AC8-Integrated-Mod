param([Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=Join-Path $repo 'package-template'
$package=[IO.Path]::GetFullPath($PackageRoot)
$zh=Get-Content -LiteralPath (Join-Path $repo 'localization/zh-CN.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$en=Get-Content -LiteralPath (Join-Path $repo 'localization/en-US.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$map=@{}
foreach($entry in $zh.messages.PSObject.Properties){$map[$entry.Value]=$en.messages.PSObject.Properties[$entry.Name].Value}
$extra=Get-Content -LiteralPath (Join-Path $repo 'localization/en-US-literals.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($entry in $extra.PSObject.Properties){$map[$entry.Name]=$entry.Value}
foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.ps1'){
 $tokens=$null;$errors=$null
 $before=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
 $after=[Management.Automation.Language.Parser]::ParseFile((Join-Path $package $file.Name),[ref]$tokens,[ref]$errors)
 if($errors.Count){throw "English parse error: $($file.Name)"}
 $a=@($before.FindAll({param($n) $true},$true));$b=@($after.FindAll({param($n) $true},$true))
 if($a.Count -ne $b.Count){throw "AST shape changed: $($file.Name)"}
 for($i=0;$i -lt $a.Count;$i++){
  if($a[$i].GetType() -ne $b[$i].GetType()){throw 'Translation changed AST node types'}
  if($a[$i] -is [Management.Automation.Language.StringConstantExpressionAst] -or $a[$i] -is [Management.Automation.Language.ExpandableStringExpressionAst]){
   if($b[$i].Value -match '[\u4e00-\u9fff]'){throw "Chinese application text remains: $($file.Name)"}
   if($a[$i].Value -notmatch '[\u4e00-\u9fff]' -and $a[$i].Value -ne '、' -and $a[$i].Value -ne $b[$i].Value){throw "Machine string changed: $($file.Name) $($a[$i].Value)"}
  }
  if($a[$i] -is [Management.Automation.Language.VariableExpressionAst] -and $a[$i].VariablePath.UserPath -ne $b[$i].VariablePath.UserPath){throw 'Variable changed'}
  if($a[$i] -is [Management.Automation.Language.CommandAst] -and $a[$i].GetCommandName() -ne $b[$i].GetCommandName()){throw 'Command changed'}
 }
}
$xml=[xml](Get-Content -LiteralPath (Join-Path $package 'Launcher-GUI.xaml') -Raw -Encoding UTF8)
foreach($node in $xml.SelectNodes('//*')){foreach($attr in $node.Attributes){if($attr.Value -match '[\u4e00-\u9fff]'){throw 'Chinese XAML text remains'}}}
$info=Get-Content -LiteralPath (Join-Path $package 'package-info.json') -Raw | ConvertFrom-Json
if($info.language -ne 'en-US' -or $info.language_switching -ne $false){throw 'Expected fixed English edition'}
Write-Host 'PASS English text coverage, PowerShell structure, machine strings and fixed-language metadata.'
# Reuse behavior regressions against the English package. Only existing language assertions
# are localized; Chinese fixture paths remain to exercise non-ASCII filesystem support.
$fixture=Join-Path $repo ('native/test-runtime/english-regression-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'tests') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fixture 'package-template') | Out-Null
foreach($entry in Get-ChildItem -LiteralPath $source -Force){
 $english=Join-Path $package $entry.Name
 if(Test-Path -LiteralPath $english){Copy-Item -LiteralPath $english -Destination (Join-Path $fixture 'package-template') -Recurse}
}
foreach($name in 'tools','models'){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $fixture -Recurse}
Copy-Item -LiteralPath (Join-Path $repo 'runtime-dependencies.json') -Destination $fixture
$suite=@('Test-Features','Test-Setup','Test-Package','Test-Cleanup','Test-InstallUX','Test-GUI','Test-CreeperUXTheme','Test-Discovery','Test-HostIsolation','Test-SteamBridge','Test-GuiLayout')
foreach($name in $suite){
 $text=Get-Content -LiteralPath (Join-Path $PSScriptRoot ($name+'.ps1')) -Raw -Encoding UTF8
 foreach($key in @('环境检查完成','已发送启动请求','操作未完成','AC8 启动失败','还没有可以分析')){
  $value=if($key -eq '还没有可以分析'){'No recorded session'}else{$map[$key]}
  $text=$text.Replace($key,$value)
 }
 $path=Join-Path $fixture ('tests/'+$name+'.ps1');[IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($true))
 $log=Join-Path $fixture ($name+'.txt')
 $ErrorActionPreference='Continue'
 & powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File $path *> $log
 $code=$LASTEXITCODE;$ErrorActionPreference='Stop'
 if($code){Get-Content -LiteralPath $log -Tail 22;throw "English regression failed: $name"}
 Write-Host "PASS English $name"
}
exit 0

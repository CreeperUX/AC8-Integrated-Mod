param([Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$Destination,[Parameter(Mandatory=$true)][string]$CatalogRoot)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Destination){throw 'Use a new translation destination.'}
Copy-Item -LiteralPath $Source -Destination $Destination -Recurse
$zh=Get-Content -LiteralPath (Join-Path $CatalogRoot 'zh-CN.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$en=Get-Content -LiteralPath (Join-Path $CatalogRoot 'en-US.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$map=@{}
foreach($entry in $zh.messages.PSObject.Properties){$map[$entry.Value]=$en.messages.PSObject.Properties[$entry.Name].Value}
$extra=Get-Content -LiteralPath (Join-Path $CatalogRoot 'en-US-literals.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($entry in $extra.PSObject.Properties){$map[$entry.Name]=$entry.Value}
$count=0
foreach($file in Get-ChildItem -LiteralPath $Destination -Filter '*.ps1'){
 $text=Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
 $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw "Invalid source: $($file.Name)"}
 $edits=@()
 foreach($node in $ast.FindAll({param($n) $n -is [Management.Automation.Language.StringConstantExpressionAst] -or $n -is [Management.Automation.Language.ExpandableStringExpressionAst]},$true)){
  if($node.Value -notmatch '[\u4e00-\u9fff]' -and $node.Value -ne '、'){continue}
  if(!$map.ContainsKey($node.Value)){throw "Missing English literal in $($file.Name): $($node.Value)"}
  $value=[string]$map[$node.Value]
  if($node -is [Management.Automation.Language.ExpandableStringExpressionAst]){
   $replacement='"'+$value.Replace('`','``').Replace('"','`"')+'"'
   $newTokens=$null;$newErrors=$null
   $newAst=[Management.Automation.Language.Parser]::ParseInput($replacement,[ref]$newTokens,[ref]$newErrors)
   if($newErrors.Count){throw 'Invalid translated expandable string'}
   $newNode=@($newAst.FindAll({param($n) $n -is [Management.Automation.Language.ExpandableStringExpressionAst]},$true))[0]
   $before=@($node.NestedExpressions | ForEach-Object {$_.Extent.Text}) -join '|'
   $after=@($newNode.NestedExpressions | ForEach-Object {$_.Extent.Text}) -join '|'
   if($before -ne $after){throw "Translation changed interpolation: $($file.Name)"}
  }else{$replacement="'"+$value.Replace("'","''")+"'"}
  $edits+=[pscustomobject]@{Start=$node.Extent.StartOffset;End=$node.Extent.EndOffset;Text=$replacement};$count++
 }
 foreach($edit in $edits | Sort-Object Start -Descending){$text=$text.Substring(0,$edit.Start)+$edit.Text+$text.Substring($edit.End)}
 $null=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw "Invalid English script: $($file.Name)"}
 [IO.File]::WriteAllText($file.FullName,$text,[Text.UTF8Encoding]::new($true))
}
$xaml=Join-Path $Destination 'Launcher-GUI.xaml'
$xml=[xml](Get-Content -LiteralPath $xaml -Raw -Encoding UTF8)
foreach($node in $xml.SelectNodes('//*')){
 foreach($attribute in @($node.Attributes)){
  if($attribute.Value -notmatch '[\u4e00-\u9fff]'){continue}
  if(!$map.ContainsKey($attribute.Value)){throw "Missing English XAML text: $($attribute.Value)"}
  $attribute.Value=$map[$attribute.Value];$count++
 }
 # English text is longer. Allow captions and mode labels to wrap without adding page scrolling.
 if($node.LocalName -eq 'TextBlock' -and !$node.HasAttribute('TextWrapping')){$node.SetAttribute('TextWrapping','Wrap')}
}
$settings=[Xml.XmlWriterSettings]::new();$settings.Indent=$true;$settings.Encoding=[Text.UTF8Encoding]::new($false)
$writer=[Xml.XmlWriter]::Create($xaml,$settings);try{$xml.Save($writer)}finally{$writer.Dispose()}
foreach($file in Get-ChildItem -LiteralPath $Destination -Filter '*.cmd'){
 $text=Get-Content -LiteralPath $file.FullName -Raw
 # Keep OS-localized pause text out of the English entrypoints.
 $text=[regex]::Replace($text,'(?m)^pause\s*$',"echo Press any key to continue . . .`r`npause >nul")
 $text=[regex]::Replace($text,'(?m)^(if .+ )pause\s*$',"`$1(echo Press any key to continue . . .`r`npause ^>nul`r`n)")
 # The redirection belongs to the command inside the group, not a literal caret sequence.
 $text=$text.Replace('pause ^>nul','pause >nul')
 [IO.File]::WriteAllText($file.FullName,$text,[Text.Encoding]::ASCII)
}
foreach($name in @('开始使用.md','验收反馈模板.txt')){
 $path=Join-Path $Destination $name
 if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}
}
Write-Host "PASS translated $count application text occurrences; embedded expressions preserved."

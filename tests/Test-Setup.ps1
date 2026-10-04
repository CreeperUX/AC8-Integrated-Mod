$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$setup=Join-Path $repo 'package-template/Setup.ps1'
foreach($file in Get-ChildItem -LiteralPath (Join-Path $repo 'package-template') -Filter '*.ps1' -File -Recurse){
 $raw=[IO.File]::ReadAllBytes($file.FullName)
 if($raw.Length -ge 6 -and [BitConverter]::ToString($raw,0,6) -eq 'EF-BB-BF-EF-BB-BF'){throw "Duplicate BOM: $($file.Name)"}
 $tokens=$null;$errors=$null
 $null=[System.Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw $errors[0]}
}
$tokens=$null;$errors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile($setup,[ref]$tokens,[ref]$errors)
if($ast.EndBlock.Statements[0] -isnot [System.Management.Automation.Language.AssignmentStatementAst]){throw 'Setup first statement must be an assignment'}
# Reproduce the old duplicate-marker bug: parsing alone accepts it as a command.
$raw=[IO.File]::ReadAllBytes($setup)
$text=[Text.Encoding]::UTF8.GetString($raw).TrimStart([char]0xFEFF)
$bad=[System.Management.Automation.Language.Parser]::ParseInput(([string][char]0xFEFF)+$text,[ref]$tokens,[ref]$errors)
if($bad.EndBlock.Statements[0] -is [System.Management.Automation.Language.AssignmentStatementAst]){throw 'Regression fixture no longer reproduces the old error'}
& {
 function Get-Process {param($Name,$ErrorAction) return $null}
 function Read-Host {param($Prompt) if($ErrorActionPreference -ne 'Stop'){throw 'Setup preference assignment failed'};throw 'TEST_SETUP_PROMPT_REACHED'}
 try{& $setup;throw 'Setup did not stop at prompt'}catch{if($_.Exception.Message -ne 'TEST_SETUP_PROMPT_REACHED'){throw}}
}
Write-Host 'PASS Setup executes its preference assignment and reaches prompt; duplicate-BOM regression and all packaged PowerShell syntax checked. No game or configuration changed.'

$ErrorActionPreference='Stop'
$session=Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'sessions') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if(!$session){throw 'No recorded session yet.'}
& python.exe -X utf8 (Join-Path $PSScriptRoot 'tools/analyze_experiment.py') --session $session.FullName
exit $LASTEXITCODE

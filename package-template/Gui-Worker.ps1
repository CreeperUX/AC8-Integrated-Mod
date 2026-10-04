param([string]$Action,[string]$Root,[string]$GamePath,[string]$SteamPath,[bool]$Missiles,[bool]$ConfirmRecovery=$false)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Gui-Core.ps1')
try{Invoke-AC8GuiAction -Action $Action -Root $Root -GamePath $GamePath -SteamPath $SteamPath -Missiles $Missiles -ConfirmRecovery $ConfirmRecovery}
catch{
 $failure=$_
 Show-AC8Problem $failure $Root $Action
 $hint=[string]$failure.Exception.Data['AC8Hint']
 if(!$hint){$hint='请查看诊断记录，核对文件归属、目录权限或其他 Mod。'}
 [pscustomobject]@{Success=$false;Title='操作未完成';Message=($failure.Exception.Message+"`n"+$hint);GameRoot=$null;Option=$null}
}

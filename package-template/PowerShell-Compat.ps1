# Resolve built-in modules from the running host, even with an empty inherited
# PSModulePath or disabled module auto-loading (including GUI runspaces).
foreach($ac8Module in @('Microsoft.PowerShell.Management','Microsoft.PowerShell.Utility')){
 $ac8ModulePath=[IO.Path]::Combine($PSHOME,'Modules',$ac8Module,($ac8Module+'.psd1'))
 Import-Module -Name $ac8ModulePath -ErrorAction Stop
}
function Enter-AC8Operation([string]$GameRoot){
 $canonical=[IO.Path]::GetFullPath($GameRoot).TrimEnd('\','/').ToUpperInvariant()
 $sha=[Security.Cryptography.SHA256]::Create()
 try{$key=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace('-','')}finally{$sha.Dispose()}
 $mutex=New-Object Threading.Mutex($false,('Local\AC8IntegratedSession-'+$key))
 $owned=$false
 try{
  try{$owned=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$owned=$true}
  if(!$owned){
   $failure=New-Object InvalidOperationException('Another AC8 session is starting or cleaning up. Wait for it to finish.')
   $failure.Data['AC8Code']='SESSION_BUSY';throw $failure
  }
  return $mutex
 }catch{$mutex.Dispose();throw}
}
function Exit-AC8Operation($Mutex){if($Mutex){try{$Mutex.ReleaseMutex()}finally{$Mutex.Dispose()}}}
# Idempotent when imported through both Install-Common and Cleanup-Core.
if(!(Test-Path Function:\Get-AC8FileHash)){
 function Get-AC8FileHash {
  param([Parameter(Mandatory=$true)][string]$LiteralPath,[ValidateSet('SHA256')][string]$Algorithm='SHA256')
  $path=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($LiteralPath)
  $stream=$null;$sha=$null
  try{
   $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
   $sha=[Security.Cryptography.SHA256]::Create()
   $digest=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')
   return [pscustomobject]@{Algorithm='SHA256';Hash=$digest;Path=$path}
  }finally{if($sha){$sha.Dispose()};if($stream){$stream.Dispose()}}
 }
}

. (Join-Path $PSScriptRoot 'Keybinding-Settings.ps1')
function Set-AC8CapturedBinding($Context,[int]$VirtualKey) {
 if(!$Context.State.Capture){return}
 $candidate=@((Get-AC8KeybindingSpec).keys.PSObject.Properties | Where-Object {$_.Value.vk -eq $VirtualKey})
 if($candidate.Count -ne 1){$Context.Status.Text='此键暂不支持或被已有快捷键保留；请换一个键，或按 Esc 取消录入。';return}
 $name=$Context.State.Capture;$Context.State.Values[$name]=[string]$candidate[0].Name
 $Context.State.Capture=$null
 $Context.Buttons[$name].Content=[string]$candidate[0].Value.label
 try{Assert-AC8Keybindings $Context.State.Values;$Context.Status.Text='已修改，点击保存后下次启动生效。';$Context.Save.IsEnabled=$true}catch{$Context.Status.Text=$_.Exception.Message;$Context.Save.IsEnabled=$false}
}
function New-AC8KeybindingsDialog([string]$Root,$Owner,[ValidateSet('dark','light')][string]$Theme='dark') {
 Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
 $xml=[xml](Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Keybindings-GUI.xaml') -Raw -Encoding UTF8)
 $dialog=[Windows.Markup.XamlReader]::Load([Xml.XmlNodeReader]::new($xml));if($Owner){$dialog.Owner=$Owner}
 Set-CreeperUXTheme $dialog $Theme
 $area=[Windows.SystemParameters]::WorkArea;$dialog.Width=[Math]::Min($dialog.Width,$area.Width-24);$dialog.Height=[Math]::Min($dialog.Height,$area.Height-24);$dialog.MinWidth=[Math]::Min($dialog.MinWidth,$dialog.Width);$dialog.MinHeight=[Math]::Min($dialog.MinHeight,$dialog.Height)
 $state=@{Values=(Read-AC8Keybindings (Join-Path $Root 'Keybindings.ini'));Capture=$null;Saved=$false}
 # Event handlers are GetNewClosure() closures, which resolve commands only at global scope. Carry the functions they
 # call on the context so the dialog works however the launcher was started (-File, call operator or dot-source).
 $commands=@{Capture=${function:Set-AC8CapturedBinding};Defaults=${function:Get-AC8DefaultBindings};Spec=${function:Get-AC8KeybindingSpec};Save=${function:Save-AC8Keybindings}}
 $ctx=[pscustomobject]@{Window=$dialog;State=$state;Buttons=@{};Status=$dialog.FindName('BindingStatus');Save=$dialog.FindName('SaveBindings');Root=$Root;Commands=$commands}
 $rows=$dialog.FindName('BindingRows')
 foreach($action in (Get-AC8KeybindingSpec).actions){
  $name=[string]$action.name;$row=[Windows.Controls.Grid]::new();$row.Margin=[Windows.Thickness]::new(0,0,0,8)
  $column=[Windows.Controls.ColumnDefinition]::new();$column.Width=[Windows.GridLength]::new(1,[Windows.GridUnitType]::Star);[void]$row.ColumnDefinitions.Add($column)
  $column=[Windows.Controls.ColumnDefinition]::new();$column.Width=[Windows.GridLength]::new(156);[void]$row.ColumnDefinitions.Add($column)
  $label=[Windows.Controls.TextBlock]::new();$label.Text=[string]$action.label;$label.VerticalAlignment='Center';$label.TextWrapping='Wrap';[void]$row.Children.Add($label)
  $button=[Windows.Controls.Button]::new();$token=[string]$state.Values[$name];$button.Content=[string](Get-AC8KeybindingSpec).keys.PSObject.Properties[$token].Value.label;$button.Tag=$name
  [Windows.Automation.AutomationProperties]::SetName($button,([string]$action.label+'：'+$button.Content));[Windows.Controls.Grid]::SetColumn($button,1);[void]$row.Children.Add($button);$ctx.Buttons[$name]=$button
  $button.Add_Click({param($sender,$event) $ctx.State.Capture=[string]$sender.Tag;$ctx.Status.Text='请按要使用的键或鼠标按钮；Esc 取消录入。';[void]$sender.Focus()}.GetNewClosure());[void]$rows.Children.Add($row)
 }
 $dialog.Add_PreviewKeyDown({param($sender,$event)
  if(!$ctx.State.Capture){return};$event.Handled=$true
  $key=$event.Key;if($key -eq [Windows.Input.Key]::System){$key=$event.SystemKey}
  if($key -eq [Windows.Input.Key]::Escape){$ctx.State.Capture=$null;$ctx.Status.Text='已取消按键录入，尚未保存。';return}
  & $ctx.Commands.Capture $ctx ([Windows.Input.KeyInterop]::VirtualKeyFromKey($key))
 }.GetNewClosure())
 $dialog.Add_PreviewMouseDown({param($sender,$event)
  if(!$ctx.State.Capture){return};$event.Handled=$true
  $vk=switch($event.ChangedButton.ToString()){'Left'{if([Windows.SystemParameters]::SwapButtons){2}else{1}} 'Right'{if([Windows.SystemParameters]::SwapButtons){1}else{2}} 'Middle'{4} 'XButton1'{5} 'XButton2'{6}}
  if($vk){& $ctx.Commands.Capture $ctx $vk}
 }.GetNewClosure())
 $dialog.FindName('RestoreBindings').Add_Click({$ctx.State.Capture=$null;$ctx.State.Values=& $ctx.Commands.Defaults;$spec=& $ctx.Commands.Spec;foreach($action in $spec.actions){$ctx.Buttons[$action.name].Content=[string]$spec.keys.PSObject.Properties[$action.default].Value.label};$ctx.Status.Text='已恢复默认；点击保存应用，或取消放弃。';$ctx.Save.IsEnabled=$true}.GetNewClosure())
 $dialog.FindName('CancelBindings').Add_Click({$ctx.State.Capture=$null;$ctx.Window.Close()}.GetNewClosure())
 $ctx.Save.Add_Click({try{if($ctx.State.Capture){throw '请先完成或取消按键录入。'};& $ctx.Commands.Save $ctx.Root $ctx.State.Values;$ctx.State.Saved=$true;$ctx.Status.Text='已保存，下次启动生效。原配置备份为 Keybindings.ini.bak。'}catch{$ctx.Status.Text=$_.Exception.Message}}.GetNewClosure())
 return $ctx
}
function Show-AC8KeybindingsDialog([string]$Root,$Owner,[string]$Theme='dark') {
 $context=New-AC8KeybindingsDialog $Root $Owner $Theme
 [void]$context.Window.ShowDialog();return [bool]$context.State.Saved
}

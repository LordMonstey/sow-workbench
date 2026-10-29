param(
 [Parameter(Mandatory=$true)][string]$Gui,
 [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
function Monstey_SowGuiHash([string]$FilePath) {
 $Monstey_Stream=[IO.File]::OpenRead($FilePath);$Monstey_Hash=$null
 try {$Monstey_Hash=[Security.Cryptography.SHA256]::Create();return [BitConverter]::ToString($Monstey_Hash.ComputeHash($Monstey_Stream)).Replace('-','').ToLowerInvariant()}
 finally {if($null-ne$Monstey_Hash){$Monstey_Hash.Dispose()};$Monstey_Stream.Dispose()}
}
$Monstey_GuiPath=[IO.Path]::GetFullPath($Gui)
if(-not[IO.File]::Exists($Monstey_GuiPath)){throw 'Explicit owned GUI executable is absent'}
$Monstey_GuiBefore=Monstey_SowGuiHash $Monstey_GuiPath
$Monstey_Run=Join-Path $OutputRoot (([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))+'-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Monstey_Run -ErrorAction Stop|Out-Null
$Monstey_Report=Join-Path $Monstey_Run 'sow-atom-gui-report.json'
$Monstey_Settings=Join-Path $Monstey_Run 'absent-settings.json'
$Monstey_Arguments=@('--settings',('"'+$Monstey_Settings+'"'),'--smoke-report',('"'+$Monstey_Report+'"'),'--smoke-sow-atom')
$Monstey_Process=$null
try {
 $Monstey_Process=Start-Process -FilePath $Monstey_GuiPath -ArgumentList $Monstey_Arguments -WindowStyle Hidden -PassThru
 if(-not$Monstey_Process.WaitForExit(55000)){throw 'Owned SoW atom GUI smoke exceeded55seconds'}
 $Monstey_Process.Refresh()
 if(-not(Test-Path -LiteralPath $Monstey_Report)){throw 'SoW atom GUI report absent'}
 $Monstey_Result=Get-Content -LiteralPath $Monstey_Report -Raw|ConvertFrom-Json
 if($Monstey_Process.ExitCode-ne0-or$Monstey_Result.status-ne'PASS'){throw ('SoW atom GUI failed: '+($Monstey_Result|ConvertTo-Json -Depth 12))}
 foreach($Monstey_Flag in @('native_gui','sow_atom_synthetic_only','sow_atom_menu_present_and_enabled',
   'sow_atom_positive_selection_history_preserved','sow_atom_raw_report_panel','sow_atom_real_menu_dispatch',
   'sow_atom_fx_guarded','sow_atom_pe_guarded_two_readers_preserved','sow_atom_no_selection_guarded',
   'sow_atom_project_history_source_selection_preserved','sow_atom_logical_new_pending_observed',
   'database_tree_root_present','database_tree_lazy_category_expanded','database_tree_record_selected',
   'database_tree_properties_loaded','database_tree_grid_synced')){
  if(-not$Monstey_Result.$Monstey_Flag){throw ('SoW atom GUI proof flag absent/false: '+$Monstey_Flag)}
 }
 if($Monstey_Result.sow_atom_requests_queued-ne3-or$Monstey_Result.sow_atom_reports_shown-ne2-or
    $Monstey_Result.sow_atom_guard_rejections-ne3-or$Monstey_Result.sow_atom_stale_callbacks_discarded-lt1){
  throw 'SoW atom menu/guard/stale dispatch counts differ'
 }
 if(-not$Monstey_Result.sow_atom_before_dirty-or-not$Monstey_Result.sow_atom_before_can_undo-or
    $Monstey_Result.sow_atom_before_can_redo){throw 'SoW atom fixture did not exercise dirty undo state'}
 $Monstey_Storage=$Monstey_Result.sow_atom_report
 if($Monstey_Storage.schema-ne'Monstey.GadbAtomUses/1'-or$Monstey_Storage.source_kind-ne'logical_memory_source'-or
    $Monstey_Storage.offset-ne0-or$Monstey_Storage.limit-ne500-or$Monstey_Storage.rows.Count-ne1-or
    $Monstey_Storage.rows[0].status-ne'not_yet_serialized'-or
    $null-ne$Monstey_Storage.rows[0].raw_u32-or$null-ne$Monstey_Storage.rows[0].atom-or
    $null-ne$Monstey_Storage.rows[0].future_serialized_count){
  throw 'SoW atom logical-New report invented serialized storage or invoked another API'
 }
 if((Test-Path -LiteralPath $Monstey_Settings)-or(Monstey_SowGuiHash $Monstey_GuiPath)-ne$Monstey_GuiBefore){
  throw 'SoW atom GUI executable/settings invariant failed'
 }
 Write-Output ('PASS native SoW storage menu/dispatch/raw-report/state/stale: '+$Monstey_Report)
} finally {
 if($null-ne$Monstey_Process-and-not$Monstey_Process.HasExited){
  if([IO.Path]::GetFullPath($Monstey_Process.Path)-eq$Monstey_GuiPath){
   $Monstey_Process.Kill();[void]$Monstey_Process.WaitForExit(5000)
  }
 }
}

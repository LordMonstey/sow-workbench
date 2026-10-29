param(
 [Parameter(Mandatory=$true)][string]$Gui,
 [Parameter(Mandatory=$true)][string]$OutputRoot,
 [ValidateSet('database_native_class','database_native_banks')][string]$Operation='database_native_class',
 [string]$Source='',
 [string]$ExpectedSha256=''
)
$ErrorActionPreference='Stop'
function Monstey_UiFileHash([string]$FilePath) {
 $stream=[IO.File]::OpenRead($FilePath);$hash=$null
 try {$hash=[Security.Cryptography.SHA256]::Create();return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}
 finally {if($null-ne$hash){$hash.Dispose()};$stream.Dispose()}
}
$run=Join-Path $OutputRoot (([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))+'-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run -ErrorAction Stop|Out-Null
$synthetic=[string]::IsNullOrEmpty($Source)
if($synthetic){
 if($Operation-ne'database_native_class'){throw 'Portable test uses the explicit native-class unsupported-image contract'}
 $Source=Join-Path $run 'synthetic-native-ui.exe'
 [IO.File]::WriteAllBytes($Source,[Text.Encoding]::ASCII.GetBytes('MZ Monstey synthetic UI input; this is not an original executable.'))
 $ExpectedSha256=Monstey_UiFileHash $Source
}else{
 $Source=[IO.Path]::GetFullPath($Source)
 if($ExpectedSha256-notmatch'^[0-9a-f]{64}$'-or(Monstey_UiFileHash $Source)-ne$ExpectedSha256){throw 'Explicit actual source/hash mismatch'}
}
$args=@{path=$Source;expected_sha256=$ExpectedSha256;offset=0;limit=8}
if($Operation-eq'database_native_class'){$args.class_pointer='0x14263c050'}else{$args.bank_index=0;$args.limit=1}
$arguments=Join-Path $run 'arguments.json'
[IO.File]::WriteAllText($arguments,($args|ConvertTo-Json -Depth 8)+"`n",[Text.UTF8Encoding]::new($false))
$report=Join-Path $run 'gui-report.json';$settings=Join-Path $run 'absent-settings.json'
$nativeArgs=@('--settings',('"'+$settings+'"'),'--smoke-report',('"'+$report+'"'),'--smoke-operation',$Operation,'--smoke-domain','database','--smoke-arguments',('"'+$arguments+'"'))
if($synthetic){$nativeArgs+=@('--smoke-expect-error','unsupported_native_image')}
$process=$null
try{
 $process=Start-Process -FilePath $Gui -ArgumentList $nativeArgs -WindowStyle Hidden -PassThru
 if(-not$process.WaitForExit(45000)){throw 'Owned native Database GUI smoke exceeded45seconds'}
 $process.Refresh()
 if(-not(Test-Path -LiteralPath $report)){throw 'GUI did not write its smoke report'}
 $r=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($process.ExitCode-ne0-or$r.status-ne'PASS'){throw ('Native Database GUI failed: '+($r|ConvertTo-Json -Depth 8))}
 if(-not$r.native_gui-or-not$r.native_database_menu_route-or-not$r.native_operation_selected-or
    -not$r.argument_editor_visible-or-not$r.native_operation_completed-or
    -not$r.native_database_source_action-or-not$r.native_database_shared_source_hash-or
    -not$r.native_database_record_routes_guarded-or-not$r.native_database_direct_dispatch-or
    -not$r.native_database_records_restored-or-not$r.native_database_project_history_preserved-or
    -not$r.native_database_mode_epoch_invalidated-or-not$r.native_database_stale_probe_queued-or
    $r.native_database_stale_callback_executed-or$r.native_dispatched_operation-ne$Operation-or
    $r.native_database_before_generation-ne$r.native_database_after_generation-or
    $r.native_database_before_revision-ne$r.native_database_after_revision-or-not$r.native_database_before_dirty){throw 'GUI route/source/stale/dirty-project proof incomplete'}
 $expectedRows=if($Operation-eq'database_native_class'){5}else{6}
 if($r.native_database_discarded_callbacks.categories-lt1-or$r.native_database_discarded_callbacks.database_native_class-lt1){throw 'Actual stale Database callbacks were not observed and rejected'}
 if($r.typed_argument_rows-ne$expectedRows-or$r.native_database_argument_types.path-ne'string'-or
    $r.native_database_argument_types.expected_sha256-ne'string'-or
    $r.native_database_argument_types.offset-ne'integer'-or$r.native_database_argument_types.limit-ne'integer') {throw 'Typed native argument form differs'}
 if($synthetic){
  if($r.native_operation_succeeded-or-not$r.native_operation_expected_error_observed-or$r.native_operation_expected_error.code-ne'unsupported_native_image'){throw 'Portable test fabricated native API success'}
 }else{
  if(-not$r.native_operation_succeeded-or$r.native_operation_expected_error_observed){throw 'Supported actual API did not succeed'}
  $format=if($Operation-eq'database_native_class'){'Monstey.GadbNativeClassInspection/1'}else{'Monstey.GadbBankInspection/1'}
  if($r.operation_result.format-ne$format){throw 'GUI invoked a different Runtime API'}
 }
 if(-not$r.source_unchanged-or(Monstey_UiFileHash $Source)-ne$ExpectedSha256-or(Test-Path -LiteralPath $settings)){throw 'GUI source/settings invariant failed'}
 Write-Output ('PASS native Database '+$(if($synthetic){'synthetic expected-error'}else{'actual supported'})+' GUI: '+$report)
}finally{
 if($null-ne$process-and-not$process.HasExited){
  if([IO.Path]::GetFullPath($process.Path)-eq[IO.Path]::GetFullPath($Gui)){$process.Kill();[void]$process.WaitForExit(5000)}
 }
}

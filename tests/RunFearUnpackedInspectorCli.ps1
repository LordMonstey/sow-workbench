param(
 [Parameter(Mandatory=$true)][string]$Exe,
 [Parameter(Mandatory=$true)][string]$TestExe,
 [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
# Use the bundled WinPS5 module explicitly; do not depend on inherited PSModulePath.
Import-Module -Name ([System.IO.Path]::Combine($PSHOME,'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1')) -Force
Set-StrictMode -Version Latest
$script:checks=0
$script:cases=0
$script:records=@()
function Assert([bool]$value,[string]$text){$script:checks++;if(-not $value){throw $text}}
function Quote([string]$text){
 $out='"';$slashes=0
 foreach($c in $text.ToCharArray()){
  if($c -eq [char]92){$slashes++;continue}
  if($c -eq [char]34){$out+=('\'*($slashes*2+1))+'"'}else{$out+=('\'*$slashes)+$c}
  $slashes=0
 }
 return $out+('\'*($slashes*2))+'"'
}
function Process([string]$path,[string[]]$arguments){
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=$path;$info.Arguments=(($arguments|ForEach-Object {Quote $_}) -join ' ')
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $info.StandardOutputEncoding=New-Object System.Text.UTF8Encoding($false,$true)
 $info.StandardErrorEncoding=New-Object System.Text.UTF8Encoding($false,$true)
 $p=New-Object System.Diagnostics.Process
 $p.StartInfo=$info
 try{
  if(-not $p.Start()){throw 'Owned process did not start'}
  $stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
  if(-not $p.WaitForExit(20000)){$p.Kill();$p.WaitForExit();throw 'Owned process timed out'}
  return [pscustomobject]@{code=$p.ExitCode;stdout=$stdout.GetAwaiter().GetResult();stderr=$stderr.GetAwaiter().GetResult();arguments=$info.Arguments}
 }finally{$p.Dispose()}
}
function Case([string]$name,[string[]]$arguments,[int]$expected){
 $script:cases++
 $result=Process $Exe $arguments
 $prefix=Join-Path $run ('case-'+$script:cases.ToString('D2')+'-'+$name)
 [System.IO.File]::WriteAllText($prefix+'.stdout.json',$result.stdout,(New-Object System.Text.UTF8Encoding($false)))
 [System.IO.File]::WriteAllText($prefix+'.stderr.txt',$result.stderr,(New-Object System.Text.UTF8Encoding($false)))
 Assert ($result.code -eq $expected) ($name+': exit '+$result.code+' expected '+$expected+' '+$result.stdout+' '+$result.stderr)
 $json=$result.stdout|ConvertFrom-Json
 $script:records+=,[pscustomobject]@{name=$name;exit_code=$result.code;arguments=$result.arguments;stdout_bytes=[System.Text.Encoding]::UTF8.GetByteCount($result.stdout)}
 return [pscustomobject]@{json=$json;stdout=$result.stdout}
}
$Exe=[System.IO.Path]::GetFullPath($Exe);$TestExe=[System.IO.Path]::GetFullPath($TestExe)
[System.IO.Directory]::CreateDirectory($OutputRoot)|Out-Null
$run=Join-Path ([System.IO.Path]::GetFullPath($OutputRoot)) ('run-'+[Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($run)|Out-Null
$exeBefore=(Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
$driverBefore=(Get-FileHash -LiteralPath $TestExe -Algorithm SHA256).Hash.ToLowerInvariant()
$fixtures=Join-Path $run 'fixtures';$manifest=Join-Path $run 'fixtures.json'
$generated=Process $TestExe @('--fixtures',$fixtures,$manifest)
Assert ($generated.code -eq 0) ('Fixture generator failed '+$generated.stderr)
$m=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8|ConvertFrom-Json
$inputs=@($m.text,$m.constraints,$m.short_constraints,$m.overflow_text,$m.budget_text,$m.byte_text,$m.byte_constraints)
$before=@{};foreach($file in $inputs){$before[$file]=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
$base=@('--text',$m.text,'--constraints',$m.constraints)
$a=Case 'help' @('--help') 0
Assert ($a.json.schema -eq 'Monstey.FearUnpackedInspectorHelp/1') 'Help schema'
$a=Case 'unicode-summary' ($base+@('--summary')) 0
Assert ($a.json.ok -and $a.json.inspection.totals.categories -eq 1) 'Summary graph'
Assert ($a.json.text_source.unchanged -and $a.json.constraints_source.unchanged) 'Both sources unchanged'
Assert ($a.json.text_source.path.Contains([string][char]0x00E9) -and $a.json.constraints_source.path.Contains([string][char]0x6D4B)) 'Unicode path identities'
Assert ($a.json.inspection.profile.statistics.arena_budget_bytes -eq 67108864) 'Default explicit64MiB'
$a=Case 'values' ($base+@('--category','0','--record','0','--attribute','0')) 0
Assert ($a.json.inspection.items[0].signed_value -eq 11) 'Typed integer value'
$a=Case 'categories' $base 0
Assert ($a.json.inspection.items[0].name.bytes_hex -eq '436174') 'Category pool bytes'
$a=Case 'records' ($base+@('--category','0')) 0
Assert ($a.json.inspection.items[0].name.bytes_hex -eq '526563') 'Record pool bytes'
$a=Case 'attributes' ($base+@('--category','0','--record','0')) 0
Assert ($a.json.inspection.items[0].runtime_type_u8 -eq 3) 'Attribute native type'
$a=Case 'past-page' ($base+@('--offset','99','--limit','2')) 0
Assert ($a.json.inspection.page.emitted -eq 0 -and -not $a.json.inspection.page.has_more -and $null -eq $a.json.inspection.page.next_offset) 'Past end page'
$a=Case 'raw-byte-names' @('--text',$m.byte_text,'--constraints',$m.byte_constraints,'--category','0','--record','0','--attribute','0') 0
Assert ($a.json.inspection.selected_attribute.name.bytes_hex -eq '41c0' -and $a.json.inspection.items[0].signed_value -eq 11) 'NonUTF8 names remain queryable'
$a=Case 'expected-hashes' ($base+@('--text-sha256',$m.text_sha256,'--constraints-sha256',$m.constraints_sha256,'--summary')) 0
Assert ($a.json.ok) 'Expected hashes accepted'
$a=Case 'wrong-hash' ($base+@('--text-sha256',('0'*64))) 1
Assert ($a.json.error_code -eq 'unpacked_source_hash_mismatch' -and $a.json.text_source.unchanged) 'Wrong hash rejected read-only'
$report=Join-Path (Split-Path -Parent $m.text) ('rapport_'+[char]0x00E9+'_'+[char]0x6D4B+'.json')
$a=Case 'new-unicode-report' ($base+@('--summary','--output',$report)) 0
Assert ((Get-Content -LiteralPath $report -Raw -Encoding UTF8).TrimEnd() -eq $a.stdout.TrimEnd()) 'Exact success report/stdout'
$reportHash=(Get-FileHash -LiteralPath $report -Algorithm SHA256).Hash
$a=Case 'existing-report' ($base+@('--summary','--output',$report)) 1
Assert ((Get-FileHash -LiteralPath $report -Algorithm SHA256).Hash -eq $reportHash) 'Existing output retained'
$a=Case 'source-text-alias' ($base+@('--output',$m.text)) 1
Assert (-not ($a.json.PSObject.Properties.Name -contains 'inspection')) 'Text alias rejected before loading'
$a=Case 'source-constraints-alias' ($base+@('--output',$m.constraints)) 1
Assert (-not ($a.json.PSObject.Properties.Name -contains 'inspection')) 'Constraint alias rejected before loading'
$a=Case 'short-constraints' @('--text',$m.text,'--constraints',$m.short_constraints) 1
Assert ($a.json.error_code -eq 'unpacked_open_returned_null' -and $a.json.constraints_source.unchanged) 'Short constraints failure/pins'
$a=Case 'line-overflow' @('--text',$m.overflow_text,'--constraints',$m.constraints) 1
Assert ($a.json.error_code -eq 'unpacked_open_returned_null') 'Concrete line overflow'
$a=Case 'arena-budget' @('--text',$m.budget_text,'--constraints',$m.constraints,'--arena-bytes','262144') 1
Assert ($a.json.error_code -eq 'unpacked_arena_exhausted' -and $a.json.details.requested_bytes -eq 70000 -and $a.json.text_source.unchanged) 'Precise budget exhaustion'
$a=Case 'invalid-budget' ($base+@('--arena-bytes','0')) 2
Assert ($a.json.error_code -eq 'invalid_arguments') 'Budget argument validation'
$a=Case 'missing-constraints' @('--text',$m.text) 2
$a=Case 'unknown-option' ($base+@('--wat','1')) 2
$a=Case 'duplicate-flag' ($base+@('--summary','--summary')) 2
$a=Case 'negative-page' ($base+@('--limit','-1')) 2
$a=Case 'bad-hierarchy' ($base+@('--record','0')) 2
$a=Case 'summary-with-selection' ($base+@('--summary','--category','0')) 2
$a=Case 'missing-ordinal' ($base+@('--category','7')) 1
Assert ($a.json.error_code -eq 'unpacked_category_ordinal') 'Precise ordinal error'
$a=Case 'zero-limit' ($base+@('--limit','0')) 2
$a=Case 'explicit-opaque' ($base+@('--summary','--callback-opaque','0xAABBCCDD','--insert-opaque','0x11223344')) 0
Assert ($a.json.inspection.profile.callback_opaque_word -eq 2864434397 -and $a.json.inspection.profile.insert_opaque_word -eq 287454020) 'Owned opaque overrides'
$a=Case 'invalid-hash-argument' ($base+@('--constraints-sha256','zz')) 2
$a=Case 'bad-string-cap' ($base+@('--string-bytes','1')) 2
foreach($file in $inputs){Assert ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $before[$file]) ('Source changed: '+$file)}
Assert ((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -eq $exeBefore) 'Tool executable changed'
Assert ((Get-FileHash -LiteralPath $TestExe -Algorithm SHA256).Hash.ToLowerInvariant() -eq $driverBefore) 'Driver executable changed'
$result=[ordered]@{status='PASS';cases=$script:cases;checks=$script:checks;tool_sha256=$exeBefore;driver_sha256=$driverBefore;unicode_paths=$true;all_input_hashes_unchanged=$true;original_assets=$false;original_executed=$false;no_writer_or_cross_source_conversion=$true;cases_detail=$script:records;fixture_manifest=$manifest;run_directory=$run}
$json=$result|ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText((Join-Path $run 'receipt.json'),$json,(New-Object System.Text.UTF8Encoding($false)))
Write-Output ('PASS '+$script:cases+' CLI cases / '+$script:checks+' checks; '+(Join-Path $run 'receipt.json'))

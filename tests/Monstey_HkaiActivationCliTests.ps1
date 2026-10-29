param(
 [Parameter(Mandatory=$true)][string]$Executable,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$LegacyExecutable=''
)
$ErrorActionPreference='Stop'
function Monstey_Hash([string]$Path) {
 $stream=[IO.File]::OpenRead($Path)
 try {
  $hash=[Security.Cryptography.SHA256]::Create()
  try {return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}
  finally {$hash.Dispose()}
 } finally {$stream.Dispose()}
}
$taskExe=(Get-Item -LiteralPath $Executable).FullName
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $taskOutput) {throw 'Fresh CLI acceptance output required'}
New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskExeHash=Monstey_Hash $taskExe
$taskLegacy=if($LegacyExecutable) {(Get-Item -LiteralPath $LegacyExecutable).FullName} else {''}
$taskLegacyHash=if($taskLegacy) {Monstey_Hash $taskLegacy} else {''}
$taskSentinel=Join-Path $taskOutput 'retained-output.json'
[IO.File]::WriteAllText($taskSentinel,'{"retained":true}',[Text.UTF8Encoding]::new($false))
$taskSentinelHash=Monstey_Hash $taskSentinel
$taskReport=Join-Path $taskOutput 'must-stay-absent\report.json'
$Monstey_ProtectedGame=Join-Path $taskOutput 'fake-game'
New-Item -ItemType Directory -Path $Monstey_ProtectedGame | Out-Null
$Monstey_ProtectedMarker=Join-Path $Monstey_ProtectedGame 'ShadowOfWar.exe'
[IO.File]::WriteAllText($Monstey_ProtectedMarker,'owned synthetic game marker',[Text.UTF8Encoding]::new($false))
$Monstey_ProtectedMarkerHash=Monstey_Hash $Monstey_ProtectedMarker
$Monstey_ProtectedOutput=Join-Path $Monstey_ProtectedGame 'protected-image-output.json'
function Monstey_QuoteArgument([AllowEmptyString()][string]$Value) {
 $builder=[Text.StringBuilder]::new()
 [void]$builder.Append([char]'"')
 $slashes=0
 foreach($character in $Value.ToCharArray()) {
  if($character -eq [char]'\') {$slashes++;continue}
  if($character -eq [char]'"') {
   [void]$builder.Append([char]'\',2*$slashes+1)
   [void]$builder.Append([char]'"')
  } else {
   [void]$builder.Append([char]'\',$slashes)
   [void]$builder.Append($character)
  }
  $slashes=0
 }
 [void]$builder.Append([char]'\',2*$slashes)
 [void]$builder.Append([char]'"')
 return $builder.ToString()
}
function Monstey_Run([string]$Exe,[AllowEmptyCollection()][string[]]$Arguments,[string]$Name) {
 $info=[Diagnostics.ProcessStartInfo]::new()
 $info.FileName=$Exe; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
 $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
 $info.StandardOutputEncoding=[Text.UTF8Encoding]::new($false)
 $info.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
 $info.Arguments=(@($Arguments | ForEach-Object {Monstey_QuoteArgument $_}) -join ' ')
 $process=[Diagnostics.Process]::new(); $process.StartInfo=$info
 try {
  if(-not $process.Start()) {throw 'Failed to start owned CLI test process'}
  $ownedPid=$process.Id
  $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
  if(-not $process.WaitForExit(10000)) {
   $timeout=@{status='FAIL_OWNED_TEST_TIMEOUT';pid=$ownedPid;command=$Exe;arguments=$Arguments}
   $timeout | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskOutput ($Name+'.timeout.json')) -Encoding UTF8
   $process.Kill(); if(-not $process.WaitForExit(5000)) {throw 'Owned test process remains live after termination request'}
   throw 'Owned CLI test exceeded timeout and was explicitly terminated'
  }
  $result=@{name=$Name;pid=$ownedPid;exit_code=$process.ExitCode;stdout=$stdout.GetAwaiter().GetResult();stderr=$stderr.GetAwaiter().GetResult();arguments=$Arguments}
  $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskOutput ($Name+'.json')) -Encoding UTF8
  return $result
 } finally {$process.Dispose()}
}
$legacyCases=@(
 @{name='usage-empty';arguments=@();message='Usage:'},
 @{name='usage-short';arguments=@('1');message='Usage:'},
 @{name='pid-zero';arguments=@('0','1',$taskReport);message='PID outside DWORD'},
 @{name='pid-overflow';arguments=@('4294967296','1',$taskReport);message='PID outside DWORD'},
 @{name='pid-negative';arguments=@('-1','1',$taskReport);message='Invalid unsigned integer'},
 @{name='creation-zero';arguments=@('1','0',$taskReport);message='Creation time must be pinned'},
 @{name='creation-malformed';arguments=@('1','0xg',$taskReport);message='Invalid unsigned integer'},
 @{name='target-overflow';arguments=@('1','1',$taskReport,'4294967296','1','2','3');message='Target hash exceeds U32'},
 @{name='point-nan';arguments=@('1','1',$taskReport,'1','nan','2','3');message='Point must be finite F32'},
 @{name='point-suffix';arguments=@('1','1',$taskReport,'1','1x','2','3');message='Point must be finite F32'},
 @{name='existing-output';arguments=@('1','1',$taskSentinel);message='Output already exists'},
 @{name='new-switch-prefix-not-matched';arguments=@('--hkai-activationX');message='Usage:'}
)
$legacyRows=@()
foreach($case in $legacyCases) {
 $new=Monstey_Run $taskExe $case.arguments ('legacy-'+$case.name)
 if($new.exit_code -ne 1 -or $new.stdout -ne '' -or -not $new.stderr.Contains($case.message)) {throw ('Legacy CLI rejection failed: '+$case.name)}
 if($taskLegacy) {
  $old=Monstey_Run $taskLegacy $case.arguments ('baseline-'+$case.name)
  if($old.exit_code -ne $new.exit_code -or $old.stdout -cne $new.stdout -or $old.stderr -cne $new.stderr) {throw ('Legacy output changed: '+$case.name)}
 }
 $legacyRows+=@{name=$case.name;status='PASS';baseline_byte_comparison=[bool]$taskLegacy}
}
$base=@('--hkai-activation','1','1','0','0','0','0',$taskReport)
$newCases=@(
 @{name='usage';arguments=@('--hkai-activation');message='WorldRuntimeProbe --hkai-activation'},
 @{name='pid-zero';arguments=@('--hkai-activation','0','1','0','0','0','0',$taskReport);message='PID outside DWORD'},
 @{name='pid-overflow';arguments=@('--hkai-activation','4294967296','1','0','0','0','0',$taskReport);message='PID outside DWORD'},
 @{name='creation-zero';arguments=@('--hkai-activation','1','0','0','0','0','0',$taskReport);message='Creation time must be pinned'},
 @{name='row-negative';arguments=@('--hkai-activation','1','1','-1','0','0','0',$taskReport);message='Invalid unsigned integer'},
 @{name='resource-overflow';arguments=@('--hkai-activation','1','1','0','18446744073709551616','0','0',$taskReport);message='Invalid unsigned integer'},
 @{name='extent-malformed';arguments=@('--hkai-activation','1','1','0','0','0x','0',$taskReport);message='Invalid unsigned integer'},
 @{name='slots-overflow';arguments=@('--hkai-activation','1','1','0','0','0','4294967296',$taskReport);message='String slot count exceeds U32'},
 @{name='slots-negative';arguments=@('--hkai-activation','1','1','0','0','0','-1',$taskReport);message='Invalid unsigned integer'},
 @{name='row-not-integer';arguments=@('--hkai-activation','1','1','1.0','0','0','0',$taskReport);message='Invalid unsigned integer'},
 @{name='lazy-zero-existing-output';arguments=@('--hkai-activation','1','1','0','0','0','0',$taskSentinel);message='Output already exists'},
 @{name='u64-max-existing-output';arguments=@('--hkai-activation','1','1','18446744073709551615','18446744073709551615','18446744073709551615','4294967295',$taskSentinel);message='Output already exists'},
 @{name='protected-image-output';arguments=@('--hkai-activation','1','1','0','0','0','0',$Monstey_ProtectedOutput);message='Write an offline output outside the game installation'}
)
$newRows=@()
foreach($case in $newCases) {
 $run=Monstey_Run $taskExe $case.arguments ('hkai-'+$case.name)
 if($run.exit_code -ne 1 -or $run.stdout -ne '' -or -not $run.stderr.Contains($case.message)) {throw ('HKAI CLI rejection failed: '+$case.name)}
 $newRows+=@{name=$case.name;status='PASS';pre_process_validation=$true}
}
$help=Monstey_Run $taskExe @('--help-hkai-activation') 'hkai-help'
if($help.exit_code -ne 0 -or $help.stderr -ne '' -or -not $help.stdout.Contains('read-safety') -or -not $help.stdout.Contains('remain unverified')) {throw 'HKAI CLI help failed'}
if(Test-Path -LiteralPath $Monstey_ProtectedOutput) {throw 'Protected game output created'}
if((Monstey_Hash $Monstey_ProtectedMarker) -ne $Monstey_ProtectedMarkerHash) {throw 'Synthetic game marker changed'}
if(Test-Path -LiteralPath (Split-Path -Parent $taskReport)) {throw 'Rejected CLI created report parent/artifact'}
if((Monstey_Hash $taskSentinel) -ne $taskSentinelHash -or (Monstey_Hash $taskExe) -ne $taskExeHash) {throw 'CLI source/sentinel changed'}
if($taskLegacy -and (Monstey_Hash $taskLegacy) -ne $taskLegacyHash) {throw 'Legacy binary changed'}
$proof=@{schema='Monstey.HkaiActivationCliAcceptance/1';status='PASS';executable=$taskExe;executable_sha256=$taskExeHash;legacy_executable_sha256=$taskLegacyHash;legacy_cases=$legacyRows;new_rejection_cases=$newRows;help_passed=$true;legacy_case_count=$legacyRows.Count;new_rejection_case_count=$newRows.Count;created_report_parent=$false;sentinel_unchanged=$true;live_process_identity_path_tested=$false;game_process_polled=$false;game_memory_written=$false;original_functions_called=$false;limits='Argument/output checks and help only; live process identity and positive HKAI capture remain untested in this driver'}
[IO.File]::WriteAllText((Join-Path $taskOutput 'result.json'),($proof|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
Write-Output ('CLI PASS: '+$legacyRows.Count+' legacy cases, '+$newRows.Count+' HKAI rejections, help; no game process calls')
param(
  [Parameter(Mandatory=$true)][string]$Gui,
  [Parameter(Mandatory=$true)][string]$TestExecutable,
  [Parameter(Mandatory=$true)][string]$BaselineCatalog,
  [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'

function Monstey_NavFileHash([string]$FilePath) {
  $stream = [IO.File]::OpenRead($FilePath)
  $algorithm = $null
  try {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','').ToLowerInvariant()
  } finally {
    if ($null -ne $algorithm) { $algorithm.Dispose() }
    $stream.Dispose()
  }
}

$run=Join-Path $OutputRoot (([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))+'-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
$fixture=Join-Path $run 'fixture'
& $TestExecutable $fixture $BaselineCatalog *> (Join-Path $run 'fixture.process.log')
if($LASTEXITCODE -ne 0){throw 'Synthetic native HKAI fixture tests failed'}
$source=Join-Path $fixture 'synthetic.hkai'
$expected=(Monstey_NavFileHash $source)
$arguments=@{path=$source;expected_sha256=$expected;limit=512}
$argumentFile=Join-Path $run 'arguments.json'
[IO.File]::WriteAllText($argumentFile,($arguments|ConvertTo-Json)+"`n",[Text.UTF8Encoding]::new($false))
$settings=Join-Path $run 'absent-settings.json'
$report=Join-Path $run 'gui-report.json'
$nativeArguments=@('--settings',('"'+$settings+'"'),'--smoke-report',('"'+$report+'"'),'--smoke-operation','sow_world_navigation_static_tree','--smoke-domain','world','--smoke-arguments',('"'+$argumentFile+'"'))
$process=$null
try {
  $process=Start-Process -FilePath $Gui -ArgumentList $nativeArguments -WindowStyle Hidden -PassThru
  if(-not $process.WaitForExit(30000)){throw 'Owned GUI smoke deadline exceeded'}
  $process.Refresh()
  if($process.ExitCode -ne 0){throw 'Owned GUI smoke failed'}
  $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
  if($result.status -ne 'PASS' -or -not $result.native_gui -or
     -not $result.native_operation_selected -or $result.typed_argument_rows -ne 5 -or
     -not $result.argument_editor_visible -or -not $result.native_operation_completed -or
     -not $result.static_tree_direct_readonly_dispatch -or -not $result.source_unchanged -or
     $result.static_tree_before_generation -ne $result.static_tree_after_generation -or
     $result.static_tree_before_revision -ne $result.static_tree_after_revision -or
     $result.operation_result.total_nodes -ne 599 -or $result.operation_result.nodes_hex.Count -ne 512){
    throw 'Native World-form readonly inspection proof incomplete'
  }
  if((Monstey_NavFileHash $source) -ne $expected -or
     (Test-Path -LiteralPath $settings)){throw 'GUI source/settings invariant failed'}
  Write-Output ('PASS native World static-tree GUI: '+$report)
} finally {
  if($null -ne $process -and -not $process.HasExited){
    if([IO.Path]::GetFullPath($process.Path) -eq [IO.Path]::GetFullPath($Gui)){$process.Kill();[void]$process.WaitForExit(5000)}
  }
}

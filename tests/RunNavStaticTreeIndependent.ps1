param(
  [Parameter(Mandatory=$true)][string]$TestExecutable,
  [Parameter(Mandatory=$true)][string]$DomainExecutable,
  [Parameter(Mandatory=$true)][string]$BaselineCatalog,
  [Parameter(Mandatory=$true)][string]$OutputRoot,
  [string]$ActualSource = ''
)
$ErrorActionPreference = 'Stop'
$taskRun = Join-Path ([System.IO.Path]::GetFullPath($OutputRoot)) ((Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskRun | Out-Null
$taskApi = Join-Path $taskRun 'api'
$taskDomain = Join-Path $taskRun 'unused-domain'
if ($ActualSource) { & $TestExecutable $taskApi $BaselineCatalog $ActualSource } else { & $TestExecutable $taskApi $BaselineCatalog }
if ($LASTEXITCODE -ne 0) { throw 'Independent API suite failed' }
& $DomainExecutable $taskDomain
if ($LASTEXITCODE -ne 0) { throw 'Unused-face API domain suite failed' }
$taskApiReport = Get-Content -LiteralPath (Join-Path $taskApi 'review.json') -Raw | ConvertFrom-Json
$taskDomainReport = Get-Content -LiteralPath (Join-Path $taskDomain 'report.json') -Raw | ConvertFrom-Json
if ($taskApiReport.status -ne 'PASS' -or $taskApiReport.groups -lt 12 -or $taskApiReport.catalog_operations -ne 227 -or $taskApiReport.world_operations -ne 41 -or $taskApiReport.old_core_descriptors_exact -ne 224 -or $taskApiReport.old_world_descriptors_exact -ne 40) { throw 'Independent API coverage is incomplete' }
if ($taskDomainReport.status -ne 'PASS' -or $taskDomainReport.groups -ne 5 -or $taskDomainReport.positive_page_cases -ne 8 -or $taskDomainReport.native_boundary_rejections -ne 3 -or $taskDomainReport.assertions -ne 103) { throw 'Unused-face coverage is incomplete' }
Write-Output ('-- Independent static-tree API and unused-face domain PASS: ' + $taskRun)

param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$requests=@(
'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"Native transport check","version":"1"}}}',
'{"jsonrpc":"2.0","method":"notifications/initialized"}',
'{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}',
'{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"strings_new","arguments":{}}}',
'{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"strings_set","arguments":{"key":"Mcp_Test","text":"Native \u03a9"}}}',
'{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"strings_entries","arguments":{}}}',
'{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"strings_remove","arguments":{"key":"Mcp_Test","expected_generation":99}}}',
'{"jsonrpc":"2.0","id":7,"method":"ping","params":{}}'
)
$responses=@($requests | & $Executable --mcp)
if($LASTEXITCODE -ne 0){throw 'Native MCP exited with an error'}
if($responses.Count -ne 7){throw 'Unexpected MCP response count'}
$decoded=@($responses | ForEach-Object { $_ | ConvertFrom-Json })
if($decoded[0].result.protocolVersion -ne '2025-03-26'){throw 'Protocol negotiation failed'}
if($decoded[1].result.tools.Count -lt 60){throw 'Native tool catalog incomplete'}
$entriesPayload=$decoded[4].result.content[0].text | ConvertFrom-Json
if($entriesPayload.result.rows[0].text -ne ('Native '+[char]0x03a9)){throw 'Native MCP Unicode roundtrip failed'}
$stalePayload=$decoded[5].result.content[0].text | ConvertFrom-Json
if(-not $decoded[5].result.isError -or $stalePayload.error.code -ne 'stale_session'){throw 'Native MCP error result failed'}
$lineResponses=@(@(('x' * 65537),'{"operation":"describe"}') | & $Executable)
if(($lineResponses[0] | ConvertFrom-Json).error.code -ne 'request_too_large'){throw 'Bounded line reader failed'}
if(-not ($lineResponses[1] | ConvertFrom-Json).ok){throw 'Bounded line recovery failed'}
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

$animationDirectory=Join-Path $OutputDirectory ('animation-cli-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssffff'))
[System.IO.Directory]::CreateDirectory($animationDirectory) | Out-Null
$animationScene=Join-Path $animationDirectory ('scene-'+[char]0x03a9+'.json')
$animationSource=Join-Path $animationDirectory 'original native.anix'
$animationCopy=Join-Path $animationDirectory 'copy native.anix'
$sceneText='{"format":"Monstey_ANIX_Semantic_v1","library_path":"characters/animationcharacters/synthetic/cli.anix","channels":[{"type_hash":"00ce66fc","bone_hash":"88baa7e7"}],"clips":[{"name":"bind","duration_ticks":1000,"ticks":[0,1000],"events":[],"tracks":[{"channel_index":0,"values":[[0,0,0],[0,0,0]]}]},{"name":"walk","duration_ticks":1000,"ticks":[0,1000],"events":[],"tracks":[{"channel_index":0,"values":[[0,0,0],[20,0,0]]}]}]}'
[System.IO.File]::WriteAllText($animationScene,$sceneText,[System.Text.UTF8Encoding]::new($false))
$animationRequests=@(
 ([ordered]@{operation='animation_new';arguments=@{scene_path=$animationScene}} | ConvertTo-Json -Compress),
 ([ordered]@{operation='animation_build';arguments=@{output=$animationSource}} | ConvertTo-Json -Compress)
)
$animationCreated=@($animationRequests | & $Executable)
if($LASTEXITCODE -ne 0 -or $animationCreated.Count -ne 2){throw 'Animation CLI creation failed'}
foreach($line in $animationCreated){if(-not ($line | ConvertFrom-Json).ok){throw $line}}
$animationReadRequests=@(
 '{"operation":"animation_sample","arguments":{"clip":"walk","ticks":[0,500,1000],"bind_fallback":false}}',
 ([ordered]@{operation='animation_build';arguments=@{output=$animationCopy}} | ConvertTo-Json -Compress)
)
$animationRead=@($animationReadRequests | & $Executable --animation $animationSource)
if($LASTEXITCODE -ne 0 -or $animationRead.Count -ne 2){throw 'Animation CLI source option failed'}
$animationSample=$animationRead[0] | ConvertFrom-Json
$animationBuilt=$animationRead[1] | ConvertFrom-Json
if(-not $animationSample.ok -or -not $animationBuilt.ok){throw 'Animation CLI inspection/build failed'}
if($animationSample.result.tracks.Count -ne 1 -or
   [Math]::Abs($animationSample.result.tracks[0].values[1][0]-10.0) -gt 0.00001){
    throw 'Animation CLI opened or sampled the wrong source'
}
if(-not $animationBuilt.result.no_op_byte_exact -or
   [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($animationSource)) -ne
   [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($animationCopy))){
    throw 'Animation CLI no-op changed native bytes'
}
$hashStream=[System.IO.File]::OpenRead($Executable)
$hashAlgorithm=[System.Security.Cryptography.SHA256]::Create()
try {$exeHash=([System.BitConverter]::ToString($hashAlgorithm.ComputeHash($hashStream))).Replace('-','').ToLowerInvariant()}
finally {$hashAlgorithm.Dispose();$hashStream.Dispose()}
$receipt=[ordered]@{
    implementation='C++20';mcp_protocol='2025-03-26';mcp_responses=7;
    tools=$decoded[1].result.tools.Count;unicode_roundtrip=$true;
    stale_session_rejected=$true;oversized_frame_recovery=$true;
    animation_source_option=$true;animation_numeric_sample=$true;animation_noop_exact=$true;
    executable_sha256=$exeHash
}
$receiptPath=Join-Path $OutputDirectory ('native-cli-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssffff')+'.json')
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8
$receipt | ConvertTo-Json -Depth 8

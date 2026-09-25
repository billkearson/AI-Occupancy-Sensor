param(
    [Parameter(Mandatory = $true)]
    [string]$DeviceIp,

    [int]$Port = 8081,
    [int]$TimeoutSec = 10
)

$ErrorActionPreference = 'Stop'

function Invoke-McpRequest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Uri,

        [Parameter(Mandatory = $true)]
        [string]$Method,

        [string]$Body = ''
    )

    try {
        if ($Method -eq 'GET') {
            $result = Invoke-RestMethod -Method Get -Uri $Uri -TimeoutSec $TimeoutSec
        }
        else {
            $result = Invoke-RestMethod -Method Post -Uri $Uri -ContentType 'application/json' -Body $Body -TimeoutSec $TimeoutSec
        }

        [pscustomobject]@{
            Name    = $Name
            Passed  = $true
            Result  = $result
            Message = 'OK'
        }
    }
    catch {
        [pscustomobject]@{
            Name    = $Name
            Passed  = $false
            Result  = $null
            Message = $_.Exception.Message
        }
    }
}

$baseUrl = "http://$DeviceIp`:$Port"
$mcpUrl = "$baseUrl/mcp"
$contextUrl = "$baseUrl/context"
$occupancyHistoryUrl = "$baseUrl/occupancy/history"

Write-Host "Testing MCP endpoints at $baseUrl" -ForegroundColor Cyan
Write-Host ""

$tests = @(
    @{ Name = 'GET /context'; Method = 'GET'; Uri = $contextUrl; Body = '' },
    @{ Name = 'GET /occupancy/history'; Method = 'GET'; Uri = $occupancyHistoryUrl; Body = '' },
    @{ Name = 'initialize'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' },
    @{ Name = 'tools/list'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' },
    @{ Name = 'tools/call get_area_data'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_area_data","arguments":{}}}' },
    @{ Name = 'tools/call get_environmental_data'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get_environmental_data","arguments":{}}}' },
    @{ Name = 'tools/call get_occupancy_data'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"get_occupancy_data","arguments":{}}}' },
    @{ Name = 'tools/call get_air_quality_data'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"get_air_quality_data","arguments":{}}}' },
    @{ Name = 'tools/call get_occupancy_history'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"get_occupancy_history","arguments":{}}}' },
    @{ Name = 'resources/list'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":8,"method":"resources/list","params":{}}' },
    @{ Name = 'resources/read room://context/current'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":9,"method":"resources/read","params":{"uri":"room://context/current"}}' },
    @{ Name = 'resources/read room://occupancy/history'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":10,"method":"resources/read","params":{"uri":"room://occupancy/history"}}' },
    @{ Name = 'ping'; Method = 'POST'; Uri = $mcpUrl; Body = '{"jsonrpc":"2.0","id":11,"method":"ping","params":{}}' }
)

$results = foreach ($t in $tests) {
    Invoke-McpRequest -Name $t.Name -Uri $t.Uri -Method $t.Method -Body $t.Body
}

$passCount = ($results | Where-Object { $_.Passed }).Count
$failCount = ($results | Where-Object { -not $_.Passed }).Count

foreach ($r in $results) {
    if ($r.Passed) {
        Write-Host "PASS  $($r.Name)" -ForegroundColor Green

        try {
            $json = $r.Result | ConvertTo-Json -Depth 10 -Compress
            if ($json.Length -gt 800) {
                $json = $json.Substring(0, 800) + '...'
            }
            Write-Host "  Response: $json"
        }
        catch {
            Write-Host "  Response: [unavailable]"
        }
    }
    else {
        Write-Host "FAIL  $($r.Name)" -ForegroundColor Red
        Write-Host "  Error: $($r.Message)"
    }

    Write-Host ""
}

Write-Host "Summary: $passCount passed, $failCount failed" -ForegroundColor Yellow

if ($failCount -gt 0) {
    exit 1
}

exit 0

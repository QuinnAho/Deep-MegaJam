<#
mcp.ps1 - tiny command-line client for Unreal 5.9's built-in MCP server.
Lets any agent or script call editor tools without a Claude MCP connection.

  mcp.ps1 ping                              initialize, print server name + toolset count
  mcp.ps1 toolsets                          list toolsets (name, tool count, summary)
  mcp.ps1 describe <Toolset>                list the tools of one toolset
  mcp.ps1 describe <Toolset> -Tool <Name>  show one tool's input schema (Toolset is Module.Class)
  mcp.ps1 call <Module.Class.Tool> '<json>' call a toolset tool (via the call_tool meta-tool)
  mcp.ps1 call <meta_tool> '<json args>'    call a top-level tool (list_toolsets, call_tool, ...)

Options: -Port (default 8000), -TimeoutSec (default 100; the server allows ~120 per call).
Images in results are saved to deep\Saved\Harness\mcp\<tool>-<timestamp>.png; base64 is never printed.
Exit code 0 only when the call succeeded (no transport error, no isError, no JSON-RPC error).
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Verb = 'help',
    [Parameter(Position = 1)][string]$Name = '',
    [Parameter(Position = 2)][string]$Json = '{}',
    [int]$Port = 8000,
    [int]$TimeoutSec = 100,
    [int]$MaxChars = 1500,
    [string]$Tool = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$OutDir   = Join-Path $RepoRoot 'deep\Saved\Harness\mcp'
$Url      = 'http://127.0.0.1:{0}/mcp' -f $Port
$script:Session = $null
$script:Id = 0
$ProtoVersion = '2025-11-25'

function Fail([string]$msg) { Write-Host ('FAIL: ' + $msg); exit 1 }

function Clip([string]$s) {
    if ($null -eq $s) { return '' }
    if ($s.Length -gt $MaxChars) { return $s.Substring(0, $MaxChars) + ('... [truncated, {0} chars total]' -f $s.Length) }
    return $s
}

# POST one JSON-RPC message. Returns the parsed JSON-RPC response object, or $null for 202/empty.
function Send-Rpc([string]$body, [int]$wantId) {
    $req = [System.Net.HttpWebRequest]::Create($Url)
    $req.Method = 'POST'
    $req.ContentType = 'application/json'
    $req.Accept = 'application/json, text/event-stream'
    $req.Timeout = $TimeoutSec * 1000
    $req.ReadWriteTimeout = $TimeoutSec * 1000
    if ($script:Session) { $req.Headers.Add('Mcp-Session-Id', $script:Session) }
    if ($script:Session) { $req.Headers.Add('MCP-Protocol-Version', $ProtoVersion) }
    $bytes = [Text.Encoding]::UTF8.GetBytes($body)
    $req.ContentLength = $bytes.Length
    $st = $req.GetRequestStream(); $st.Write($bytes, 0, $bytes.Length); $st.Close()
    try {
        $resp = $req.GetResponse()
    } catch [System.Net.WebException] {
        $resp = $_.Exception.Response
        if ($null -eq $resp) { Fail ('no response from ' + $Url + ': ' + $_.Exception.Message) }
    }
    $sid = $resp.Headers['Mcp-Session-Id']
    if ($sid) { $script:Session = $sid }
    $ctype = [string]$resp.ContentType
    $rd = New-Object IO.StreamReader($resp.GetResponseStream(), [Text.Encoding]::UTF8)
    $text = $rd.ReadToEnd(); $rd.Close()
    $code = [int]$resp.StatusCode
    $resp.Close()
    if ([string]::IsNullOrWhiteSpace($text)) {
        if ($code -ge 400) { Fail ('HTTP ' + $code) }
        return $null
    }
    $payloads = @()
    if ($ctype -match 'event-stream') {
        $cur = ''
        foreach ($line in ($text -split "`n")) {
            $l = $line.TrimEnd("`r")
            if ($l.StartsWith('data:')) { $cur += $l.Substring(5).TrimStart() }
            elseif ($l -eq '' -and $cur -ne '') { $payloads += $cur; $cur = '' }
        }
        if ($cur -ne '') { $payloads += $cur }
    } else {
        $payloads += $text
    }
    $found = $null
    foreach ($p in $payloads) {
        try { $o = $p | ConvertFrom-Json } catch { continue }
        if ($o.PSObject.Properties['id'] -and $o.id -eq $wantId) { $found = $o }
    }
    if ($null -eq $found) { Fail ('HTTP {0}, no JSON-RPC reply with id {1}: {2}' -f $code, $wantId, (Clip $text)) }
    return $found
}

function Invoke-Rpc([string]$method, $params) {
    $script:Id++
    $msg = [ordered]@{ jsonrpc = '2.0'; id = $script:Id; method = $method }
    if ($null -ne $params) { $msg['params'] = $params }
    $r = Send-Rpc ($msg | ConvertTo-Json -Depth 30 -Compress) $script:Id
    if ($r.PSObject.Properties['error'] -and $r.error) {
        Fail ('JSON-RPC error {0}: {1}' -f $r.error.code, (Clip ([string]$r.error.message)))
    }
    return $r.result
}

function Connect-Mcp {
    $init = @{
        protocolVersion = $ProtoVersion
        capabilities    = @{}
        clientInfo      = @{ name = 'deep-mcp-ps1'; version = '1.0' }
    }
    $res = Invoke-Rpc 'initialize' $init
    $note = '{"jsonrpc":"2.0","method":"notifications/initialized"}'
    $null = Send-Rpc $note -1
    return $res
}

# Run a tools/call, print compact output, save images. Returns $true on success.
function Show-ToolResult($res, [string]$label) {
    $ok = $true
    if ($res.PSObject.Properties['isError'] -and $res.isError) { $ok = $false }
    foreach ($c in @($res.content)) {
        if ($c.type -eq 'text') {
            Write-Host (Clip ([string]$c.text))
        } elseif ($c.type -eq 'image') {
            if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }
            $ext = 'png'
            if ($c.mimeType -match 'jpe?g') { $ext = 'jpg' }
            $safe = ($label -replace '[^A-Za-z0-9_-]', '_')
            $file = Join-Path $OutDir ('{0}-{1}.{2}' -f $safe, (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), $ext)
            [IO.File]::WriteAllBytes($file, [Convert]::FromBase64String([string]$c.data))
            Write-Host ('image saved: ' + $file)
        } else {
            Write-Host ('[{0} content, not shown]' -f $c.type)
        }
    }
    if ($res.PSObject.Properties['structuredContent'] -and $res.structuredContent -and -not @($res.content).Count) {
        Write-Host (Clip ($res.structuredContent | ConvertTo-Json -Depth 8 -Compress))
    }
    return $ok
}

function Get-Text($res) {
    $t = ''
    foreach ($c in @($res.content)) { if ($c.type -eq 'text') { $t += [string]$c.text } }
    return $t
}

function Call-Meta([string]$tool, $arguments) {
    return Invoke-Rpc 'tools/call' @{ name = $tool; arguments = $arguments }
}

function ConvertTo-Hash($obj) {
    # PS 5.1 ConvertFrom-Json gives PSCustomObject; ConvertTo-Json round-trips it fine, so pass through.
    return $obj
}

if ($Verb -eq 'help') {
    Get-Content -LiteralPath $PSCommandPath -TotalCount 16 | ForEach-Object { $_ }
    exit 0
}

$init = Connect-Mcp
$serverName = $init.serverInfo.name

switch ($Verb) {
    'ping' {
        $res = Call-Meta 'list_toolsets' @{}
        $txt = Get-Text $res
        $n = @(($txt -split "`n") | Where-Object { $_ -match '^- [A-Za-z0-9_.]+: ' }).Count
        Write-Host ('OK server={0} version={1} protocol={2} session={3} toolsets={4} port={5}' -f $serverName, $init.serverInfo.version, $init.protocolVersion, [bool]$script:Session, $n, $Port)
        exit 0
    }
    'toolsets' {
        $res = Call-Meta 'list_toolsets' @{}
        $txt = Get-Text $res
        foreach ($l in ($txt -split "`n")) {
            if ($l -match '^- ([A-Za-z0-9_.]+): (.*)$') {
                $d = $Matches[2].TrimEnd("`r"); if ($d.Length -gt 110) { $d = $d.Substring(0, 110) + '...' }
                Write-Host ('{0}: {1}' -f $Matches[1], $d)
            }
        }
        if ($res.isError) { exit 1 }
        exit 0
    }
    'describe' {
        if (-not $Name) { Fail 'usage: describe <Toolset> | <Toolset.Tool>' }
        if ($Tool) { $Name = $Name + '.' + $Tool }
        if ($Tool) {
            $res = Call-Meta 'describe_tools' @{ tool_names = @($Name) }
        } else {
            $res = Call-Meta 'describe_toolsets' @{ toolset_names = @($Name) }
        }
        $ok = Show-ToolResult $res 'describe'
        if ($ok) { exit 0 } else { exit 1 }
    }
    'call' {
        if (-not $Name) { Fail 'usage: call <Toolset.Tool> ''<json args>''' }
        try { $args1 = $Json | ConvertFrom-Json } catch { Fail ('arguments are not valid JSON: ' + $_.Exception.Message) }
        $meta = @('list_toolsets', 'describe_toolsets', 'describe_tools', 'call_tool')
        if ($meta -contains $Name) {
            $res = Call-Meta $Name $args1
        } elseif ($Name.Contains('.')) {
            $i = $Name.LastIndexOf('.')
            $res = Call-Meta 'call_tool' @{ toolset_name = $Name.Substring(0, $i); tool_name = $Name.Substring($i + 1); arguments = $args1 }
        } else {
            $res = Call-Meta 'call_tool' @{ tool_name = $Name; arguments = $args1 }
        }
        $ok = Show-ToolResult $res ($Name -replace '^.*\.', '')
        if ($ok) { exit 0 }
        Write-Host 'FAIL: tool reported isError'
        exit 1
    }
    default { Fail ('unknown verb ' + $Verb) }
}


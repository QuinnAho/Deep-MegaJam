<#
playtest.ps1 - scripted playtests for DEEP through the editor's MCP server. Costs no model tokens.

  playtest.ps1 <scenario|all> [-Port 8012] [-KeepEditor]

Scenarios (the game since the submarine room, Nov 15):
  S0-title    boot to the title, any key starts, standing in the sub room
  S1-hatch    drop through the hatch, grab real salvage, come back up, bank at the console
  S2-gate     the helm stops at 200 m without the flashlight, passes after buying it, parks at stop 2
  S3-serpent  at stop 2 the sea serpent is present
  S4-cut      cut tether + bailout air, back aboard (fee), then death and respawn aboard
  S5-harpoon  the harpoon reels a cut diver into the hatch
  S6-bank     the settlement numbers and the automatic gate purchase
  S7-run      full run: stop 1, flashlight, stop 2, hull upgrade, stop 3, relic (reached by teleport, brought home by
              winch with the lamp on the serpent), win screen
  S10-relic   swim down the sinkhole by keys, take the relic, winch it home with R held; when the serpent stalks,
              bites or hunts within 30 m, aim the lamp at its head (Deep.Look at the state report's serpent.head_cm);
              on a cut, harpoon the sub if within 25 m, else drop the relic and swim home. -NoLamp: lamp off (danger check)
  S10-cutdrop the relic aboard, cut at the bottom of the hole: drop it and swim home on bailout air (lamp defence)
  S8-keys     stop 1 on simulated keys only (walk to hatch, grab with E, console, helm, hold C), then lamp/winch/cut/harpoon/pause
Starts the editor if the port does not answer, runs PIE per scenario, asserts on DeepToolset.GetState JSON
(and the DEEPMONSTER log line for the serpent), saves a viewport PNG per scenario to
deep\Saved\Harness\playtest\<scenario>.png and devlog\media\2026-11-15-play-<scenario>.png, stops PIE and prints
PASS or FAIL <scenario>: <reason>. Exit code 0 only if everything passed.
Keys: W/A/S/D move, Space up/jump, C down, E use/grab, G drop, R winch, F lamp, LMB harpoon (ADeepPlayerController).
#>
param(
    [Parameter(Position = 0)][string]$Scenario = 'all',
    [int]$Port = 8012,
    [switch]$KeepEditor,
    [double]$Speed = 1,         # game-time speed: slomo n, and every game-time wait and key hold shortened by n
    [switch]$NoLamp,            # S10: the scripted player never uses the lamp (the danger check)
    [switch]$NoUnthrottle       # leave the editor's background CPU throttle on when this script starts the editor
)
$ErrorActionPreference = 'Stop'
$Tools = $PSScriptRoot
$Repo = Split-Path -Parent $PSScriptRoot
$ShotDir = Join-Path $Repo 'deep\Saved\Harness\playtest'
$MediaDir = Join-Path $Repo 'devlog\media'
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
New-Item -ItemType Directory -Force -Path $MediaDir | Out-Null
$All = @('S0-title', 'S1-hatch', 'S2-gate', 'S3-serpent', 'S4-cut', 'S5-harpoon', 'S6-bank', 'S7-run', 'S8-keys', 'S9-floor', 'S10-relic', 'S10-cutdrop', 'S11-pass', 'S12-sidehome')
$script:Notes = @()

# ---- MCP in-process: one HTTP session for the whole run (the same protocol as tools\mcp.ps1) ----------
$McpUrl = 'http://127.0.0.1:{0}/mcp' -f $Port
$McpProto = '2025-11-25'
$script:McpSession = $null
$script:McpId = 0
[Net.ServicePointManager]::Expect100Continue = $false
function Mcp-Post([string]$body, [int]$timeoutSec) {
    $req = [Net.HttpWebRequest]::Create($McpUrl)
    $req.Method = 'POST'; $req.ContentType = 'application/json'; $req.Accept = 'application/json, text/event-stream'
    $req.Timeout = $timeoutSec * 1000; $req.ReadWriteTimeout = $timeoutSec * 1000
    if ($script:McpSession) { $req.Headers.Add('Mcp-Session-Id', $script:McpSession); $req.Headers.Add('MCP-Protocol-Version', $McpProto) }
    $bytes = [Text.Encoding]::UTF8.GetBytes($body)
    $req.ContentLength = $bytes.Length
    $st = $req.GetRequestStream(); $st.Write($bytes, 0, $bytes.Length); $st.Close()
    try { $resp = $req.GetResponse() } catch [Net.WebException] { $resp = $_.Exception.Response; if ($null -eq $resp) { throw } }
    $sid = $resp.Headers['Mcp-Session-Id']; if ($sid) { $script:McpSession = $sid }
    $ctype = [string]$resp.ContentType
    $rd = New-Object IO.StreamReader($resp.GetResponseStream(), [Text.Encoding]::UTF8)
    $text = $rd.ReadToEnd(); $rd.Close(); $resp.Close()
    return [pscustomobject]@{ Ctype = $ctype; Text = $text }
}
function Mcp-Rpc([string]$method, $params, [int]$timeoutSec, [bool]$wantReply = $true) {
    $msg = [ordered]@{ jsonrpc = '2.0'; method = $method }
    if ($wantReply) { $script:McpId++; $msg['id'] = $script:McpId }
    if ($null -ne $params) { $msg['params'] = $params }
    $r = Mcp-Post ($msg | ConvertTo-Json -Depth 30 -Compress) $timeoutSec
    if (-not $wantReply) { return $null }
    $payloads = @()
    if ($r.Ctype -match 'event-stream') {
        $cur = ''
        foreach ($line in ($r.Text -split "`n")) {
            $l = $line.TrimEnd("`r")
            if ($l.StartsWith('data:')) { $cur += $l.Substring(5).TrimStart() } elseif ($l -eq '' -and $cur -ne '') { $payloads += $cur; $cur = '' }
        }
        if ($cur -ne '') { $payloads += $cur }
    } else { $payloads += $r.Text }
    foreach ($p in $payloads) {
        try { $o = $p | ConvertFrom-Json } catch { continue }
        if ($o.PSObject.Properties['id'] -and $o.id -eq $script:McpId) {
            if ($o.PSObject.Properties['error'] -and $o.error) { throw ('JSON-RPC error {0}: {1}' -f $o.error.code, $o.error.message) }
            return $o.result
        }
    }
    throw ('no JSON-RPC reply: ' + $r.Text.Substring(0, [Math]::Min(200, $r.Text.Length)))
}
function Mcp-Connect([int]$timeoutSec) {
    $script:McpSession = $null
    $res = Mcp-Rpc 'initialize' @{ protocolVersion = $McpProto; capabilities = @{}; clientInfo = @{ name = 'deep-playtest'; version = '2.0' } } $timeoutSec
    $null = Mcp-Rpc 'notifications/initialized' $null $timeoutSec $false
    return $res
}
# Same contract as before: Ok, and Text (the tool's text; images are saved and reported as 'image saved: <path>').
function Mcp([string]$verb, [string]$name, [string]$json) {
    try {
        if ($verb -eq 'ping') {
            $res = Mcp-Connect 3
            return [pscustomobject]@{ Ok = $true; Text = ('OK server=' + $res.serverInfo.name) }
        }
        if (-not $script:McpSession) { $null = Mcp-Connect 30 }
        $args1 = $null; if ($json) { $args1 = $json | ConvertFrom-Json } else { $args1 = @{} }
        $i = $name.LastIndexOf('.')
        $p = @{ name = 'call_tool'; arguments = @{ toolset_name = $name.Substring(0, $i); tool_name = $name.Substring($i + 1); arguments = $args1 } }
        try { $res = Mcp-Rpc 'tools/call' $p 100 }
        catch [Net.WebException] { $script:McpSession = $null; $null = Mcp-Connect 30; $res = Mcp-Rpc 'tools/call' $p 100 }   # session lost: reconnect once
        $ok = -not ($res.PSObject.Properties['isError'] -and $res.isError)
        $t = ''
        foreach ($c in @($res.content)) {
            if ($c.type -eq 'text') { $t += [string]$c.text + "`n" }
            elseif ($c.type -eq 'image') {
                $dir = Join-Path $Repo 'deep\Saved\Harness\mcp'
                New-Item -ItemType Directory -Force -Path $dir | Out-Null
                $ext = 'png'; if ($c.mimeType -match 'jpe?g') { $ext = 'jpg' }
                $f = Join-Path $dir ('{0}-{1}.{2}' -f ($name -replace '^.*\.', ''), (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), $ext)
                [IO.File]::WriteAllBytes($f, [Convert]::FromBase64String([string]$c.data))
                $t += 'image saved: ' + $f + "`n"
            }
        }
        return [pscustomobject]@{ Ok = $ok; Text = $t.Trim() }
    } catch {
        return [pscustomobject]@{ Ok = $false; Text = ('transport: ' + $_.Exception.Message) }
    }
}
function Call([string]$tool, [string]$json) {
    # The editor refuses tools while it runs a slow task (the first load of big assets, shader work) and says the
    # call made no changes, so a retry is safe. Wait for it, up to a minute, rather than fail the scenario: on
    # Nov 15 S10-relic and S3-serpent each failed once this way, on a cold editor and on the serpent's first load.
    for ($try = 0; ; $try++) {
        $r = Mcp 'call' $tool $json
        if ($r.Ok) { return $r }
        if ($r.Text -match 'running a slow task' -and $try -lt 30) { Start-Sleep -Milliseconds 2000; continue }
        throw ('{0} failed: {1}' -f $tool, $r.Text)
    }
}
function Get-State {
    $r = Call 'DeepToolset.DeepToolset.GetState' '{}'
    return (($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json)
}
function Run-Exec([string]$cmd) {
    $r = Call 'DeepToolset.DeepToolset.Exec' ('{"Command":"' + $cmd + '"}')
    return (($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json)
}
function Get-Arena {
    $r = Call 'DeepToolset.DeepToolset.GetArena' '{}'
    return (($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json)
}
# The serpent is not in GetState; Deep.Monster.State logs one DEEPMONSTER {json} line to the editor log.
function Get-Monster {
    $log = Join-Path $Repo 'deep\Saved\Logs\Deep.log'
    $mark = (Get-Item -LiteralPath $log).Length
    $null = Run-Exec 'Deep.Monster.State'
    for ($i = 0; $i -lt 10; $i++) {
        Wait-Sec 0.5
        $fs = [IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
        try {
            if ($fs.Length -gt $mark) {
                $fs.Seek($mark, 'Begin') | Out-Null
                $txt = (New-Object IO.StreamReader($fs)).ReadToEnd()
                $m = [regex]::Matches($txt, 'DEEPMONSTER (\{.*\})')
                if ($m.Count -gt 0) { return ($m[$m.Count - 1].Groups[1].Value | ConvertFrom-Json) }
            }
        } finally { $fs.Dispose() }
    }
    return $null
}
# keys: array of key names, held together for $sec seconds
function Hold([string[]]$keys, [double]$sec) {
    $e = @(); foreach ($k in $keys) { $e += ('{"key":{"keyName":"' + $k + '"},"value":{"x":1,"y":0,"z":0}}') }
    $j = '{"entries":[' + ($e -join ',') + '],"durationSeconds":' + ([Math]::Max(0.05, $sec / $Speed)).ToString([Globalization.CultureInfo]::InvariantCulture) + '}'
    $t0 = [Diagnostics.Stopwatch]::StartNew()
    $null = Call 'InputToolset.InputToolset.ApplyInput' $j
    # the tool may return before the keys are released; a hold is over only when its time is
    $left = [Math]::Max(0.05, $sec / $Speed) - $t0.Elapsed.TotalSeconds
    if ($left -gt 0) { Start-Sleep -Milliseconds ([int]($left * 1000)) }
    $script:HoldWaited += [Math]::Max(0, $left)
}
$script:HoldWaited = 0
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([Math]::Max(20, [int]($s * 1000 / $Speed))) }   # a wait for game time: shorter when the game runs faster
function Need([bool]$cond, [string]$why) { if (-not $cond) { throw $why } }
function Teleport($p) { $null = Run-Exec ('Deep.Teleport {0} {1} {2}' -f [int]$p[0], [int]$p[1], [int]$p[2]); Wait-Sec 0.5 }
function Near([double]$a, [double]$b, [double]$tol) { return ([Math]::Abs($a - $b) -le $tol) }

function Wait-Editor([int]$timeoutSec) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $timeoutSec) {
        $r = Mcp 'ping' '' ''
        if ($r.Ok -and $r.Text -match '^OK') { return $true }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

function Start-Pie {
    $null = Call 'EditorToolset.EditorAppToolset.StartPIE' '{}'
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 25) {
        Start-Sleep -Milliseconds 250
        $r = Mcp 'call' 'DeepToolset.DeepToolset.GetState' '{}'
        if ($r.Ok) {
            $s = ($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json
            if ($s.diver_present) {
                if ($Speed -ne 1) { $null = Run-Exec ('slomo ' + $Speed.ToString([Globalization.CultureInfo]::InvariantCulture)) }
                Start-Sleep -Milliseconds 400; return
            }
        }
    }
    throw 'PIE started but no diver appeared within 25 s'
}
function Stop-Pie {
    $null = Mcp 'call' 'EditorToolset.EditorAppToolset.StopPIE' '{}'
    # poll until the game world is gone (GetState fails or reports no diver), not a fixed 2 s
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 8) {
        $r = Mcp 'call' 'DeepToolset.DeepToolset.GetState' '{}'
        if (-not $r.Ok) { break }
        try { $s = ($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json; if (-not $s.diver_present) { break } } catch { break }
        Start-Sleep -Milliseconds 150
    }
}
# One viewport PNG: kept in the harness folder and in devlog\media (named for the dev log).
function Shot([string]$name) {
    $r = Mcp 'call' 'SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport' '{"world":null,"maxResolution":1280}'
    if ($r.Ok -and $r.Text -match 'image saved: (.+\.png)') {
        $src = $Matches[1].Trim()
        Copy-Item -LiteralPath $src -Destination (Join-Path $ShotDir ($name + '.png')) -Force
        Copy-Item -LiteralPath $src -Destination (Join-Path $MediaDir ('2026-11-15-play2-' + $name + '.png')) -Force
    } else { $script:Notes += ($name + ': screenshot failed: ' + $r.Text) }
}

# Poll GetState until $test returns true or $timeoutSec real seconds pass; returns the last state.
function Wait-State([scriptblock]$test, [double]$timeoutSec, [string]$what) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $s = Get-State
        if (& $test $s) { return $s }
        if ($sw.Elapsed.TotalSeconds -gt $timeoutSec) { throw ('timed out after {0:N0} s waiting for {1}; run_state={2} credits={3} stop={4} sub_depth={5} inside={6}' -f $sw.Elapsed.TotalSeconds, $what, $s.run_state, $s.credits, $s.stop, $s.sub_depth_m, $s.inside) }
        Start-Sleep -Milliseconds 120
    }
}
# Leave the settlement screen the way a player does (Space), retrying until the minimum screen time has passed.
function Press-Continue {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 40) {
        $s = Get-State
        if ($s.run_state -ne 'settling') { break }
        Hold @('SpaceBar') 0.3
        Wait-Sec 0.5
    }
    $null = Wait-State { param($s) $s.run_state -ne 'settling' } 40 'the settlement screen to close'
}
# From the title: press a key like a player; fall back to Deep.Start (noted) if the key did not reach the game.
function Begin-Run {
    $s = Get-State
    if ($s.on_title) {
        Hold @('SpaceBar') 0.3
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ((Get-State).on_title -and $sw.Elapsed.TotalSeconds -lt 1.5) { Start-Sleep -Milliseconds 100 }
        if ((Get-State).on_title) { $script:Notes += 'title: a key press did not start the run, used Deep.Start'; $null = Run-Exec 'Deep.Start' }
    }
    $null = Wait-State { param($s) -not $s.on_title -and $s.run_state -eq 'aboard' } 15 'the run to start aboard'
}
function Dive {
    $null = Run-Exec 'Deep.Hatch'
    $null = Wait-State { param($s) $s.run_state -eq 'tethered' -and -not $s.inside } 10 'the dive through the hatch'
}
# Swim well away from the hatch before a cut: a cut diver at the hatch is simply re-tethered at once (the game's rule).
function Far-Out {
    $anc = (Get-Arena).anchor_cm
    for ($i = 0; $i -lt 4; $i++) {
        Teleport @(($anc[0] + 2500), $anc[1], ($anc[2] - 300))
        if ((Get-State).tether_distance_m -gt 15) { return $anc }
        $script:Notes += 'teleport far out did not take, retrying'
    }
    throw 'Deep.Teleport never put the diver 25 m from the hatch'
}
function Drive-Now([string]$where) {
    $null = Run-Exec ('Deep.Drive {0} now' -f $where)
    Wait-Sec 0.5
    return (Get-State)
}
# Grab a real piece of salvage near the hatch (the arena is re-spawned per dive, so ask again on every try).
function Grab-Real {
    for ($try = 0; $try -lt 3 -and (Get-State).cargo_kg -eq 0; $try++) {
        $a = Get-Arena
        $p = $a.salvage[0].pos_cm
        Teleport @(($p[0] - 80), $p[1], $p[2])
        Hold @('E') 0.4
        Wait-Sec 0.7
    }
    return (Get-State)
}

# ---- scenarios -------------------------------------------------------------------------------
function S0-title {
    $s = Get-State
    Need ($s.on_title) 'boot did not show the title (on_title false)'
    $script:Notes += ('S0 title: run_state={0} inside={1} stop={2} sub_depth={3}' -f $s.run_state, $s.inside, $s.stop, $s.sub_depth_m)
    Shot 'S0-title-screen'
    Begin-Run
    $s = Get-State; $a = Get-Arena
    $script:Notes += ('S0 started: run_state={0} inside={1} parked_stop={2} sub_depth={3} limit={4} pos_cm={5}' -f $s.run_state, $s.inside, $s.parked_stop, $s.sub_depth_m, $s.depth_limit_m, ($s.pos_cm -join ','))
    Need (-not $s.on_title) 'still on the title after the start'
    Need ($s.inside) 'the diver is not inside the submarine after the start'
    Need ($s.run_state -eq 'aboard') ('run_state is ' + $s.run_state + ', expected aboard')
    Need ($null -ne $a.submarine) 'no submarine in the world'
    Need ($s.stop -eq 1 -and $s.parked_stop -eq 1) ('stop {0} parked_stop {1}, expected the sub parked at stop 1' -f $s.stop, $s.parked_stop)
    Need (-not $s.seated) 'the diver starts seated at the helm'
    Need ($s.credits -eq 0 -and $s.hull_rating -eq 1 -and -not $s.has_flashlight) ('start account: credits {0} hull {1} flashlight {2}' -f $s.credits, $s.hull_rating, $s.has_flashlight)
    Need ($s.depth_limit_m -eq 200) ('depth limit {0}, expected 200 without the flashlight' -f $s.depth_limit_m)
    # inside the room: the diver is within a few metres of the sub
    $sub = $a.submarine
    $d = [Math]::Sqrt([Math]::Pow($s.pos_cm[0] - $sub[0], 2) + [Math]::Pow($s.pos_cm[1] - $sub[1], 2) + [Math]::Pow($s.pos_cm[2] - $sub[2], 2)) / 100
    $script:Notes += ('S0 diver {0:N1} m from the sub origin' -f $d)
    Need ($d -lt 12) ('diver is {0:N1} m from the submarine, not in the room' -f $d)
    Hold @('W') 1.5
    $s2 = Get-State
    $moved = [Math]::Sqrt([Math]::Pow($s2.pos_cm[0] - $s.pos_cm[0], 2) + [Math]::Pow($s2.pos_cm[1] - $s.pos_cm[1], 2)) / 100
    Need ($moved -gt 0.5) ('W moved the diver only {0:N2} m aboard' -f $moved)
}
function S1-hatch {
    Begin-Run
    # down through the hatch
    $null = Run-Exec 'Deep.Hatch'
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 10 'the dive'
    $script:Notes += ('S1 dived: inside={0} depth={1:N1} tether={2} m dist={3:N1}' -f $s.inside, $s.depth_m, $s.tether_length_m, $s.tether_distance_m)
    Need (-not $s.inside) 'still inside after the hatch drop'
    Need ($s.tether_length_m -gt 0) 'no tether after the drop'
    Need ($s.depth_m -gt 90) ('depth {0:N1} m after the drop, expected near the 100 m stop' -f $s.depth_m)
    Shot 'S1-hatch-under'
    # a real grab
    $s = Grab-Real
    $script:Notes += ('S1 grab: cargo_kg={0} items={1} salvage_in_world={2}' -f $s.cargo_kg, $s.cargo_items, $s.salvage_in_world)
    Need ($s.cargo_kg -gt 0) 'cargo stayed 0 kg after pressing E beside a salvage piece'
    $val = $s.cargo_value
    Need ($val -gt 0) 'grabbed cargo has no value'
    # back up through the hatch
    $null = Run-Exec 'Deep.EnterSub'
    $s = Wait-State { param($s) $s.inside } 10 'coming back aboard'
    $script:Notes += ('S1 back aboard: run_state={0} cargo_kg={1}' -f $s.run_state, $s.cargo_kg)
    $script:Notes += ('S1 audio: last={0} loops={1} muted={2}' -f $s.audio.last, (@($s.audio.loops) -join ','), $s.audio.muted)
    Need ($null -ne $s.audio) 'state report has no audio key (sound layer missing)'
    Need ([bool]$s.audio.last) 'no sound fired during a dive, grab and return (audio.last is empty)'
    Need (@($s.audio.loops).Count -ge 1) 'no sound loop is playing (expected an ambient loop)'
    Need ($s.run_state -eq 'aboard') ('run_state ' + $s.run_state + ' after the return')
    Need ($s.cargo_kg -gt 0) 'cargo was lost on the way up'
    # bank at the console
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling at the console'
    $b = $s.last_bank
    $script:Notes += ('S1 banked: gross {0} cut {1} fees {2} net {3} credits {4}' -f $b.gross, $b.cut, $b.fees, $b.net, $s.credits)
    Need ($b.gross -eq $val) ('gross {0}, expected the cargo value {1}' -f $b.gross, $val)
    Need ($s.cargo_kg -eq 0) 'cargo not emptied by the bank'
    Need ($s.credits -ge 0) 'credits negative'
    Shot 'S1-hatch-bank'
    Press-Continue
    $s = Get-State
    Need ($s.inside -and $s.run_state -eq 'aboard') ('after the settlement: run_state {0} inside {1}' -f $s.run_state, $s.inside)
}
function S2-gate {
    Begin-Run
    $null = Run-Exec 'Deep.Credits 0'
    $s = Drive-Now 's2'
    $script:Notes += ('S2 no flashlight: sub_depth {0:N1} limit {1} parked_stop {2} stop {3} last_event {4}' -f $s.sub_depth_m, $s.depth_limit_m, $s.parked_stop, $s.stop, $s.last_event)
    Need ($s.sub_depth_m -le 201) ('the helm went to {0:N1} m without the flashlight (limit should be 200)' -f $s.sub_depth_m)
    Need ($s.sub_depth_m -gt 120) ('the helm barely moved: {0:N1} m' -f $s.sub_depth_m)
    Need ($s.parked_stop -ne 2 -and $s.stop -eq 1) ('arrived at stop 2 without the flashlight: parked_stop {0} stop {1}' -f $s.parked_stop, $s.stop)
    # buy the flashlight at the console (50 cr)
    $null = Run-Exec 'Deep.Credits 50'
    $null = Run-Exec 'Deep.Console'
    $s = Get-State
    $script:Notes += ('S2 bought: flashlight {0} credits {1} limit {2}' -f $s.has_flashlight, $s.credits, $s.depth_limit_m)
    Need ($s.has_flashlight) 'the console did not sell the flashlight for 50 credits'
    Need ($s.credits -eq 0) ('credits {0} after paying 50' -f $s.credits)
    Need ($s.depth_limit_m -eq 400) ('limit {0}, expected 400' -f $s.depth_limit_m)
    # back to stop 1 at once, then drive down to stop 2 for real (the helm frame is taken on the way)
    $s = Drive-Now 's1'
    Need ($s.parked_stop -eq 1) ('could not return to stop 1: sub_depth {0:N1}' -f $s.sub_depth_m)
    $null = Run-Exec 'Deep.Drive s2'
    $null = Wait-State { param($s) $s.sub_depth_m -gt 140 } 60 'the helm to get moving'
    Shot 'S2-gate-helm'
    $s = Wait-State { param($s) $s.parked_stop -eq 2 } 120 'parking at stop 2'
    $script:Notes += ('S2 stop 2: sub_depth {0:N1} stop {1} parked_stop {2} seated {3}' -f $s.sub_depth_m, $s.stop, $s.parked_stop, $s.seated)
    Need ($s.stop -eq 2) ('stop is {0}, expected 2' -f $s.stop)
    Need (Near $s.sub_depth_m 300 2) ('parked at {0:N1} m, expected 300' -f $s.sub_depth_m)
}
function S3-serpent {
    Begin-Run
    $null = Run-Exec 'Deep.BuyGate free'
    $s = Drive-Now 's2'
    Need ($s.parked_stop -eq 2) ('could not park at stop 2: sub_depth {0:N1}, limit {1}' -f $s.sub_depth_m, $s.depth_limit_m)
    $null = Run-Exec 'Deep.Hatch'
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 10 'the dive at stop 2'
    $m = $null
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 40) {
        $m = Get-Monster
        if ($m -and $m.present) { break }
    }
    if ($m) { $script:Notes += ('S3 serpent: present={0} state={1} stop={2} tether_distance={3} m diver_distance={4} m' -f $m.present, $m.state, $m.stop, $m.tether_distance_m, $m.diver_distance_m) }
    Need ($null -ne $m) 'could not read DEEPMONSTER from the editor log'
    Need ($m.present) ('no serpent at stop 2 after {0:N0} s of diving (state {1})' -f $sw.Elapsed.TotalSeconds, $m.state)
    Need ($m.stop -eq 2) ('serpent reports stop {0}' -f $m.stop)
}
function S4-cut {
    Begin-Run
    $null = Run-Exec 'Deep.Credits 100'
    Dive
    # branch A: Patience runs out, the line is cut, the bailout air counts down, the diver swims home
    $null = Far-Out
    $null = Run-Exec 'Deep.SetPatience 0'
    $s = Wait-State { param($s) $s.run_state -eq 'cut' } 30 'Patience to cut the tether'
    $a0 = $s.bailout_air_s
    Need ($a0 -gt 0) 'bailout air is 0 right after the cut'
    Wait-Sec 3
    $s = Get-State
    $script:Notes += ('S4 cut: air {0:N1} -> {1:N1} s, state {2}' -f $a0, $s.bailout_air_s, $s.run_state)
    Need ($s.bailout_air_s -lt $a0) ('bailout air did not count down: {0} -> {1}' -f $a0, $s.bailout_air_s)
    Shot 'S4-cut-bailout'
    $c0 = $s.credits
    $null = Run-Exec 'Deep.EnterSub'
    $s = Wait-State { param($s) $s.inside } 10 'coming aboard after the cut'
    $script:Notes += ('S4 aboard: state {0} credits {1} -> {2}' -f $s.run_state, $c0, $s.credits)
    Need ($s.run_state -eq 'aboard') ('after the cut, back aboard run_state is ' + $s.run_state)
    Need ($s.credits -eq [Math]::Max(0, $c0 - 20)) ('credits {0}, expected {1} after the 20 re-tether fee' -f $s.credits, ($c0 - 20))
    # branch B: cut again with cargo, let the air run out
    $n0 = $s.diver_number; $cb = $s.credits
    Dive
    $null = Far-Out
    $null = Run-Exec 'Deep.GiveSalvage 5 50'
    $null = Run-Exec 'Deep.ForceCut'
    $s = Wait-State { param($s) $s.run_state -eq 'cut' } 10 'forced cut'
    $s = Wait-State { param($s) $s.run_state -eq 'dead' -or $s.diver_number -gt $n0 } 100 'bailout air to run out (death)'
    $script:Notes += ('S4 death: diver {0} -> {1} credits {2} -> {3} state {4}' -f $n0, $s.diver_number, $cb, $s.credits, $s.run_state)
    $s = Wait-State { param($s) $s.diver_number -gt $n0 -and $s.run_state -eq 'aboard' } 30 'respawn aboard'
    Need ($s.diver_number -eq ($n0 + 1)) ('diver number {0}, expected {1}' -f $s.diver_number, ($n0 + 1))
    Need ($s.credits -eq [Math]::Max(0, $cb - 30)) ('credits {0} after death, expected {1} (30 fee)' -f $s.credits, [Math]::Max(0, $cb - 30))
    Need ($s.cargo_kg -eq 0) ('cargo is {0} kg after respawn' -f $s.cargo_kg)
    Need ($s.inside) 'the new diver did not respawn inside the submarine'
}
function S5-harpoon {
    Begin-Run
    Dive
    $null = Run-Exec 'Deep.Harpoon.Reset'
    $null = Far-Out
    $null = Run-Exec 'Deep.ForceCut'
    $s = Wait-State { param($s) $s.run_state -eq 'cut' } 10 'forced cut'
    $c0 = $s.credits
    $s = Get-State
    $script:Notes += ('S5 before: run_state {0} inside {1} dist {2:N1} m harpoon {3}' -f $s.run_state, $s.inside, $s.tether_distance_m, ($s.harpoon | ConvertTo-Json -Compress))
    Need (-not $s.inside) 'diver was already inside before the shot'
    $null = Run-Exec 'Deep.Harpoon.AimSub fire'
    Wait-Sec 0.5
    $h = (Get-State).harpoon
    $script:Notes += ('S5 fired: harpoon {0}' -f ($h | ConvertTo-Json -Compress))
    Shot 'S5-harpoon-reel'
    $s = Wait-State { param($s) $s.inside } 60 'the harpoon to reel the diver into the hatch'
    $script:Notes += ('S5 reeled: run_state {0} inside {1} credits {2} -> {3} harpoon {4}' -f $s.run_state, $s.inside, $c0, $s.credits, ($s.harpoon | ConvertTo-Json -Compress))
    Need ($s.inside) 'the diver was not brought aboard'
    Need ($s.run_state -eq 'aboard') ('run_state ' + $s.run_state + ' after the reel')
}
function S6-bank {
    Begin-Run
    # A: tiny haul where fees exceed the take: net clamps at zero
    $null = Run-Exec 'Deep.GiveSalvage 1 5'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling (small bank)'
    $script:Notes += ('S6 small: gross {0} cut {1} fees {2} net {3} credits {4}' -f $s.last_bank.gross, $s.last_bank.cut, $s.last_bank.fees, $s.last_bank.net, $s.credits)
    Need ($s.last_bank.gross -eq 5) ('gross {0}, expected 5' -f $s.last_bank.gross)
    Need ($s.last_bank.net -ge 0) ('net went negative: {0}' -f $s.last_bank.net)
    Need ($s.credits -eq [Math]::Max(0, $s.last_bank.net)) ('credits {0} after a net of {1}' -f $s.credits, $s.last_bank.net)
    Press-Continue
    # B: gross 100: cut 40, fees 10 percent + 5 = 15, net 45 (below the 50 flashlight price)
    $c0 = (Get-State).credits
    $null = Run-Exec 'Deep.GiveSalvage 10 100'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling'
    $b = $s.last_bank
    $script:Notes += ('S6 bank: gross {0} cut {1} fees {2} net {3} credits {4} -> {5} flashlight {6}' -f $b.gross, $b.cut, $b.fees, $b.net, $c0, $s.credits, $s.has_flashlight)
    Need ($b.gross -eq 100) ('gross {0}, expected 100' -f $b.gross)
    Need ($b.cut -eq [int][Math]::Ceiling(100 * 0.4)) ('cut {0}, expected 40 (40 percent rounded up)' -f $b.cut)
    Need ($b.fees -eq 15) ('fees {0}, expected 15 (10 percent + 5)' -f $b.fees)
    Need ($b.net -eq 45) ('net {0}, expected 45' -f $b.net)
    Need ($s.credits -eq ($c0 + 45)) ('credits {0}, expected {1} + 45' -f $s.credits, $c0)
    Need (-not $s.has_flashlight) 'flashlight bought with 45 credits (costs 50)'
    Need ($s.cargo_kg -eq 0) 'cargo not emptied by the bank'
    Press-Continue
    # C: gross 300: net 145, the flashlight (50) is bought by the settlement
    $null = Run-Exec 'Deep.Credits 0'
    $null = Run-Exec 'Deep.GiveSalvage 20 300'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling (300)'
    $script:Notes += ('S6 big: net {0} bought={1} flashlight {2} limit {3} credits {4}' -f $s.last_bank.net, $s.last_bank.hull_bought, $s.has_flashlight, $s.depth_limit_m, $s.credits)
    Need ($s.last_bank.net -eq 145) ('net {0}, expected 145' -f $s.last_bank.net)
    Need ($s.last_bank.hull_bought -and $s.has_flashlight) 'the flashlight was not bought with 145 net credits'
    Need ($s.credits -eq 95) ('credits {0}, expected 95' -f $s.credits)
    Need ($s.depth_limit_m -eq 400) ('limit {0}, expected 400' -f $s.depth_limit_m)
    # frame of the settlement screen
}
function S7-run {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Begin-Run
    # stop 1: a real dive and a real grab, then top up so the bank buys the flashlight
    Dive
    $s = Grab-Real
    Need ($s.cargo_kg -gt 0) 'grab failed at stop 1'
    $null = Run-Exec 'Deep.GiveSalvage 20 300'
    $null = Run-Exec 'Deep.EnterSub'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling at stop 1'
    $script:Notes += ('S7 bank 1: net {0} flashlight {1} credits {2} at {3:N0}s' -f $s.last_bank.net, $s.has_flashlight, $s.credits, $sw.Elapsed.TotalSeconds)
    Need ($s.has_flashlight) 'flashlight not bought'
    Press-Continue
    # stop 2
    $s = Drive-Now 's2'
    Need ($s.parked_stop -eq 2) ('stop 2 not reached with the flashlight: sub_depth {0:N1} limit {1}' -f $s.sub_depth_m, $s.depth_limit_m)
    Dive
    $null = Run-Exec 'Deep.GiveSalvage 20 700'
    $null = Run-Exec 'Deep.EnterSub'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling at stop 2'
    $script:Notes += ('S7 bank 2: net {0} hull_upgraded {1} credits {2} at {3:N0}s' -f $s.last_bank.net, $s.hull_upgraded, $s.credits, $sw.Elapsed.TotalSeconds)
    Need ($s.hull_upgraded) 'hull upgrade not bought (200 cr)'
    Need ($s.depth_limit_m -gt 400) ('limit {0}, expected the rest of the depth open' -f $s.depth_limit_m)
    Press-Continue
    # stop 3 and the relic
    $s = Drive-Now 's3'
    Need ($s.parked_stop -eq 3) ('stop 3 not reached with the hull upgrade: sub_depth {0:N1} limit {1}' -f $s.sub_depth_m, $s.depth_limit_m)
    Dive
    $null = Run-Exec 'Deep.Relic'
    Wait-Sec 0.5
    $null = Run-Exec 'Deep.Grab'
    Wait-Sec 0.5
    $s = Get-State
    $script:Notes += ('S7 relic: carrying={0} distance {1:N1} m cargo {2} kg' -f $s.carrying_relic, $s.relic_distance_m, $s.cargo_kg)
    Need ($s.carrying_relic) ('the relic was not picked up (relic_in_world {0}, distance {1:N1} m)' -f $s.relic_in_world, $s.relic_distance_m)
    # home the way a player comes home: winch (R held), the lamp on the serpent when it closes in (no teleport)
    $anc = (Get-Arena).anchor_cm
    $null = Run-Exec 'Deep.Lamp 1'
    $r = Ascend-With-Relic $anc 'S7'
    $s = Get-State
    $script:Notes += ('S7 ascent: inside {0} carrying {1} run_state {2}; {3}' -f $s.inside, $s.carrying_relic, $s.run_state, $r)
    Need ($s.inside -and $s.carrying_relic) ('the relic did not come home: inside {0} carrying {1} run_state {2}; {3}' -f $s.inside, $s.carrying_relic, $s.run_state, $r)
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.won } 15 'the win'
    $script:Notes += ('S7 won: rating {0} kept {1} divers {2} at {3:N0}s' -f $s.run_totals.rating, $s.run_totals.kept, $s.run_totals.divers_expended, $sw.Elapsed.TotalSeconds)
    Need ($s.won) 'bank of the relic did not win'
    Need ($null -ne $s.run_totals) 'no run totals on the win screen'
    Wait-Sec 1
}

# ---- S8-keys helpers: drive by keys, steer with the positions the state report gives -------------------
function Yaw-Of($dx, $dy) { return [Math]::Atan2($dy, $dx) * 180 / [Math]::PI }
function Look-At($from, $to) {
    $dx = $to[0] - $from[0]; $dy = $to[1] - $from[1]; $dz = $to[2] - $from[2]
    $h = [Math]::Sqrt($dx * $dx + $dy * $dy)
    $pitch = [Math]::Atan2($dz, [Math]::Max($h, 1)) * 180 / [Math]::PI
    $null = Run-Exec ('Deep.Look {0:N1} {1:N1}' -f $pitch, (Yaw-Of $dx $dy))
}
# Walk on the deck with W (facing the target via Deep.Look) until $stop says so or the point is within $tol cm.
function Walk-To($target, [double]$tol, [double]$maxSec, [scriptblock]$stop) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $maxSec) {
        $s = Get-State
        if ($stop -and (& $stop $s)) { return $s }
        $dx = $target[0] - $s.pos_cm[0]; $dy = $target[1] - $s.pos_cm[1]
        $d = [Math]::Sqrt($dx * $dx + $dy * $dy)
        if ($d -le $tol) { return $s }
        $null = Run-Exec ('Deep.Look 0 {0:N1}' -f (Yaw-Of $dx $dy))
        Hold @('W') ([Math]::Min(0.7, [Math]::Max(0.15, ($d - $tol) / 320)))
    }
    return (Get-State)
}
function Dist3($a, $b) { return [Math]::Sqrt([Math]::Pow($a[0]-$b[0],2) + [Math]::Pow($a[1]-$b[1],2) + [Math]::Pow($a[2]-$b[2],2)) }
function Walk-Heading {
    $a = (Get-State).pos_cm; Hold @('W') 0.6; $b = (Get-State).pos_cm
    return (Yaw-Of ($b[0] - $a[0]) ($b[1] - $a[1]))
}
function Find-Finding([string]$text) { $script:Notes += ('FINDING ' + $text) }

function S8-keys {
    $setup = @()
    # 1. title and a key: the real key path
    $s = Get-State
    Need ($s.on_title) 'boot did not show the title'
    Hold @('SpaceBar') 0.3
    Wait-Sec 1
    $s = Get-State
    if ($s.on_title) { Find-Finding 'TITLE: SpaceBar did not leave the title'; $null = Run-Exec 'Deep.Start'; $setup += 'Deep.Start' }
    $s = Wait-State { param($s) -not $s.on_title -and $s.run_state -eq 'aboard' } 15 'the run to start aboard'
    $script:Notes += ('S8 started on a key: inside {0} parked_stop {1} pos {2}' -f $s.inside, $s.parked_stop, ($s.pos_cm -join ','))
    $sub = (Get-Arena).submarine
    $L = { param($lx, $ly) @(($sub[0] + $lx), ($sub[1] + $ly)) }
    Shot 'S8-1-title-key'
    # 1b. real mouse look: does a Mouse2D / MouseX push turn the view? Measured by the heading of a short walk.
    $null = Run-Exec 'Deep.Look 0 0'
    $h0 = Walk-Heading
    $turn = $null
    foreach ($k in @('Mouse2D', 'MouseX')) {
        $null = Run-Exec 'Deep.Look 0 0'
        $e = '{"entries":[{"key":{"keyName":"' + $k + '"},"value":{"x":40,"y":0,"z":0}}],"durationSeconds":0.3}'
        try { $null = Call 'InputToolset.InputToolset.ApplyInput' $e } catch { $script:Notes += ('S8 mouse ' + $k + ' rejected: ' + $_.Exception.Message.Substring(0, [Math]::Min(120, $_.Exception.Message.Length))); continue }
        $h1 = Walk-Heading
        $script:Notes += ('S8 mouse look via {0}: heading before {1:N0} deg, after a push of x=40: {2:N0} deg' -f $k, $h0, $h1)
        if ([Math]::Abs($h1 - $h0) -gt 5) { $turn = $k; break }
    }
    if ($null -eq $turn) { Find-Finding 'MOUSE LOOK: no simulated Mouse2D/MouseX push turned the view (input tool or game: see notes)' }
    # 2. walk to the hatch (local 0,0) and drop through it
    $s = Walk-To (& $L 0 0) 30 25 { param($s) $s.run_state -ne 'aboard' }
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 10 'falling through the hatch after walking to it'
    $script:Notes += ('S8 dropped by walking: depth {0:N1} m inside {1}' -f $s.depth_m, $s.inside)
    Shot 'S8-2-under'
    # 3. swim to the nearest salvage, grab with E
    Need ((Get-Arena).salvage.Count -gt 0) 'no salvage in the arena'
    for ($leg = 0; $leg -lt 40; $leg++) {
        $s = Get-State
        if ($s.cargo_kg -gt 0) { break }
        $best = $null; $bd = 1e12
        foreach ($p in (Get-Arena).salvage) { $d = Dist3 $p.pos_cm $s.pos_cm; if ($d -lt $bd) { $bd = $d; $best = $p } }
        Need ($null -ne $best) 'salvage vanished'
        Look-At $s.pos_cm $best.pos_cm
        if ($bd -le 130) { Hold @('E') 0.3; Wait-Sec 0.5 }
        else { Hold @('W') ([Math]::Min(1.2, [Math]::Max(0.2, ($bd - 110) / 300))) }
    }
    $s = Get-State
    $script:Notes += ('S8 grab with E: cargo_kg {0} value {1} (legs {2})' -f $s.cargo_kg, $s.cargo_value, $leg)
    Need ($s.cargo_kg -gt 0) 'E never picked salvage up while swimming up to the nearest piece'
    # 4. back up through the hatch (aim at the anchor, W plus Space)
    $anc = (Get-Arena).anchor_cm
    for ($leg = 0; $leg -lt 40; $leg++) {
        $s = Get-State
        $script:Notes += ('S8 climb leg {0}: run_state {1} inside {2} items {3} credits {4} dist {5:N1}' -f $leg, $s.run_state, $s.inside, $s.cargo_items, $s.credits, $s.tether_distance_m)
        if ($s.inside) { break }
        # the hull is solid: get under the hatch first, then rise straight up through it
        $hd = [Math]::Sqrt([Math]::Pow($s.pos_cm[0] - $anc[0], 2) + [Math]::Pow($s.pos_cm[1] - $anc[1], 2))
        if ($hd -gt 70) {
            $under = @($anc[0], $anc[1], ($anc[2] - 220))
            if ($s.pos_cm[2] -gt $anc[2] - 150) { $under[2] = $anc[2] - 250 }
            Look-At $s.pos_cm $under
            Hold @('W') ([Math]::Min(0.8, [Math]::Max(0.15, $hd / 350)))
        } else {
            Look-At $s.pos_cm @($anc[0], $anc[1], ($anc[2] + 150))
            Hold @('W', 'SpaceBar') 0.5
        }
    }
    $s = Get-State
    $script:Notes += ('S8 back up by keys: inside {0} run_state {1} cargo {2} kg' -f $s.inside, $s.run_state, $s.cargo_kg)
    Need ($s.inside -and $s.run_state -eq 'aboard') ('did not come back aboard by swimming up (inside {0}, run_state {1}, dist {2:N1} m)' -f $s.inside, $s.run_state, $s.tether_distance_m)
    # 5. walk to the console, E; credits are setup so the bank can afford the flashlight gate
    $null = Run-Exec 'Deep.Credits 60'; $setup += 'Deep.Credits 60'
    $s = Walk-To (& $L -160 175) 120 25 { param($s) $s.near_console }
    Need ($s.near_console) 'walked to the console but near_console is false'
    $cash0 = $s.credits
    $script:Notes += ('S8 at console: credits {0} cargo_kg {1} cargo_items {2} near_console {3} run_state {4}' -f $s.credits, $s.cargo_kg, $s.cargo_items, $s.near_console, $s.run_state)
    Hold @('E') 0.3
    $trace = @(); $seen = $false
    for ($i = 0; $i -lt 16; $i++) { $x = Get-State; $trace += ('{0}/{1}/{2}' -f $x.run_state, $x.credits, $x.cargo_items); if ($x.run_state -eq 'settling') { $seen = $true; $s = $x; break } }
    $script:Notes += ('S8 console E trace (state/credits/items): ' + ($trace -join ' '))
    Need $seen 'E at the console never opened the settlement'
    $script:Notes += ('S8 console E: settling, gross {0} net {1} credits {2} -> {3} flashlight {4}' -f $s.last_bank.gross, $s.last_bank.net, $cash0, $s.credits, $s.has_flashlight)
    Shot 'S8-5-settle'
    # 5b. E on the settlement screen: leaves it, and must not use the console again
    Wait-Sec 1.6
    Hold @('E') 0.3
    $log = @()
    for ($i = 0; $i -lt 4; $i++) { $x = Get-State; $log += ('{0}/{1}' -f $x.run_state, $x.credits); Wait-Sec 0.4 }
    $script:Notes += ('S8 E on settlement: run_state/credits after the press: ' + ($log -join ' '))
    $s = Get-State
    if ($s.run_state -eq 'settling') { Find-Finding ('SETTLEMENT E: still settling after E (E did not continue, or the console was used again): ' + ($log -join ' ')) }
    $s = Wait-State { param($s) $s.run_state -eq 'aboard' } 6 'the settlement screen to close on E'
    Need ($s.has_flashlight) ('flashlight not bought by the settlement (credits {0})' -f $s.credits)
    # 6. walk to the helm, E, every helm key, then the autopilot to stop 2 (300 m)
    # the open hatch lies on the straight line from console to helm: walk around it (a diver who steps in drops into the sea)
    $null = Walk-To (& $L 0 175) 45 15 $null
    $s = Walk-To (& $L 370 0) 130 25 { param($s) $s.near_helm }
    $hp = (& $L 370 0)
    $script:Notes += ('S8 helm walk ended: pos {0} helm_world {1} dist_xy {2:N0} cm near_helm {3} sub {4}' -f ($s.pos_cm -join ','), ($hp -join ','), [Math]::Sqrt([Math]::Pow($s.pos_cm[0]-$hp[0],2)+[Math]::Pow($s.pos_cm[1]-$hp[1],2)), $s.near_helm, ($sub -join ','))
    Need ($s.near_helm) 'walked to the helm but near_helm is false'
    Hold @('E') 0.3
    $s = Wait-State { param($s) $s.seated } 5 'E at the helm to sit'
    $script:Notes += ('S8 helm E: seated {0} sub_depth {1:N1}' -f $s.seated, $s.sub_depth_m)
    # Every helm key once: C floods the ballast lever and it stays, X is all stop, W/S step
    # the throttle a notch a press, A holds the rudder. The descent to stop 2 is then the autopilot (setup): a scripted
    # key-only pilot through the bends is not written yet, and a sub that drops straight down hits the bends.
    $t0 = $s.sub_depth_m
    Hold @('C') 1.2
    $s = Get-State
    $script:Notes += ('S8 held C 1.2 s: ballast {0:N2} throttle_notch {1}' -f $s.ballast, $s.throttle_notch)
    Need ($s.ballast -gt 0.5) ('holding C did not flood the ballast lever (ballast {0})' -f $s.ballast)
    Wait-Sec 6
    $s = Get-State
    $script:Notes += ('S8 6 s on a flooded lever: sub_depth {0:N1} -> {1:N1}, helm_speed {2:N2} m/s, ballast {3:N2}' -f $t0, $s.sub_depth_m, $s.helm_speed_mps, $s.ballast)
    Need ($s.sub_depth_m -gt $t0 + 6) 'a flooded ballast lever did not take the submarine down'
    Need ($s.ballast -gt 0.5) 'the ballast lever did not stay where it was left'
    Shot 'S8-6-helm'
    Hold @('X') 0.3
    Wait-Sec 0.3
    $s = Get-State
    $script:Notes += ('S8 X: ballast {0:N2} throttle_notch {1} sub_depth {2:N1}' -f $s.ballast, $s.throttle_notch, $s.sub_depth_m)
    Need ([Math]::Abs($s.ballast) -lt 0.05 -and $s.throttle_notch -eq 0) ('X did not centre the levers (ballast {0:N2}, notch {1})' -f $s.ballast, $s.throttle_notch)
    Hold @('W') 0.3
    Wait-Sec 0.3
    $n1 = (Get-State).throttle_notch
    Hold @('W') 0.3
    Wait-Sec 0.3
    $n2 = (Get-State).throttle_notch
    Hold @('S') 0.3
    Hold @('S') 0.3
    Wait-Sec 0.3
    $n0 = (Get-State).throttle_notch
    $script:Notes += ('S8 throttle by W, W, S, S: notch {0} -> {1} -> {2}' -f $n1, $n2, $n0)
    Need ($n1 -eq 1 -and $n2 -eq 2 -and $n0 -eq 0) ('W/S did not step the throttle a notch a press ({0}, {1}, {2})' -f $n1, $n2, $n0)
    $y0 = (Get-State).sub_yaw_deg
    Hold @('D') 2
    Wait-Sec 0.5
    $s = Get-State
    $script:Notes += ('S8 rudder D held 2 s: sub_yaw {0:N1} -> {1:N1} deg' -f $y0, $s.sub_yaw_deg)
    Need ($s.sub_yaw_deg -gt $y0 + 10) 'holding D did not turn the submarine to starboard'
    Hold @('X') 0.3
    $null = Run-Exec 'Deep.GotoStop 2'; $setup += 'Deep.GotoStop 2'
    $s = Wait-State { param($s) $s.parked_stop -eq 2 } 90 'the autopilot to park at stop 2'
    Wait-Sec 2.5
    $s = Get-State
    $script:Notes += ('S8 parked: sub_depth {0:N1} parked_stop {1} stop {2} seated {3} yaw {4:N1}' -f $s.sub_depth_m, $s.parked_stop, $s.stop, $s.seated, $s.sub_yaw_deg)
    Need ($s.parked_stop -eq 2) ('the sub did not park at stop 2 (parked_stop {0})' -f $s.parked_stop)

    # ---- extras: each non-fatal, a FINDING line when it fails ----
    function Extra([string]$name, [scriptblock]$body) { try { & $body } catch { Find-Finding ($name + ': ' + ($_.Exception.Message -replace '\s+', ' ')) } }
    Extra 'E at helm to stand up' { Hold @('E') 0.3; Wait-Sec 0.6; $x = Get-State; $script:Notes += ('S8 E to leave helm: seated {0}' -f $x.seated); if ($x.seated) { Find-Finding 'HELM: E did not stand the diver up' } }
    Extra 'drop through the hatch at stop 2' {
        $null = Walk-To (& $L 0 0) 30 30 { param($s) $s.run_state -ne 'aboard' }
        $null = Wait-State { param($s) $s.run_state -eq 'tethered' } 10 'the second drop'
    }
    Extra 'lamp F' {
        $x0 = (Get-State).lamp; Hold @('F') 0.3; Wait-Sec 0.4; $x1 = (Get-State).lamp
        $script:Notes += ('S8 lamp F: {0} -> {1}' -f $x0, $x1)
        if ($x0 -eq $x1) { Find-Finding 'LAMP: F did not toggle the lamp (flashlight owned)' }
    }
    Extra 'winch R' {
        $null = Far-Out
        $d0 = (Get-State).tether_distance_m; Hold @('R') 2.5; $x = Get-State
        $script:Notes += ('S8 winch R 2.5 s: tether_distance {0:N1} -> {1:N1} m, run_state {2}' -f $d0, $x.tether_distance_m, $x.run_state)
        if ($x.tether_distance_m -ge $d0 - 1) { Find-Finding 'WINCH: holding R did not reel the diver in' }
    }
    Extra 'cut Q' {
        $null = Far-Out
        $null = Run-Exec 'Deep.Harpoon.Reset'
        Hold @('Q') 2.3; Wait-Sec 0.5; $x = Get-State
        $script:Notes += ('S8 cut Q held 2.3 s: run_state {0} bailout_air {1:N1}' -f $x.run_state, $x.bailout_air_s)
        if ($x.run_state -ne 'cut') { Find-Finding ('CUT: holding Q 2.3 s did not cut the tether (run_state ' + $x.run_state + ')') }
    }
    Extra 'harpoon LMB' {
        $x = Get-State; Need ($x.run_state -eq 'cut') 'not cut, nothing to shoot from'
        $subp = (Get-Arena).submarine
        Look-At $x.pos_cm @($subp[0], $subp[1], ($subp[2] - 50))
        Hold @('LeftMouseButton') 0.25
        Wait-Sec 0.6
        $script:Notes += ('S8 LMB fired: harpoon ' + ((Get-State).harpoon | ConvertTo-Json -Compress))
        $x = Wait-State { param($s) $s.inside } 60 'the harpoon to reel the diver aboard'
        $script:Notes += ('S8 harpoon reeled: run_state {0}' -f $x.run_state)
    }
    Extra 'pause Esc' {
        $r0 = (Get-State).run_s; Wait-Sec 1; $r1 = (Get-State).run_s
        Hold @('Escape') 0.3; Wait-Sec 0.5; $p0 = (Get-State).run_s; Wait-Sec 1.5; $p1 = (Get-State).run_s
        $script:Notes += ('S8 pause: run_s unpaused {0:N1}->{1:N1}, after Esc {2:N1}->{3:N1}' -f $r0, $r1, $p0, $p1)
        if (($p1 - $p0) -gt 1.0) { Find-Finding 'PAUSE: Esc via the input tool did not pause (the tool skips PlayerController::InputKey, where the menu keys are read)'; return }
        Hold @('Escape') 0.3; Wait-Sec 0.5; $q0 = (Get-State).run_s; Wait-Sec 1; $q1 = (Get-State).run_s
        $script:Notes += ('S8 resume: run_s {0:N1}->{1:N1}' -f $q0, $q1)
        if (($q1 - $q0) -lt 0.5) { Find-Finding 'PAUSE: second Esc did not resume' }
    }
    $script:Notes += ('S8 console commands used: ' + ($setup -join '; ') + '; Deep.Look (aim), Deep.Teleport (Far-Out), Deep.Harpoon.Reset; reads GetState and GetArena')
}


# ---- the Nov 15 night's changes: floor salvage, sinkhole relic, first pass, side return ----------------
# Swim by keys toward a point (cm): face it, hold W. Throws with the state values when the diver stops making progress.
function Swim-To($target, [double]$tolCm, [int]$maxLegs, [string]$what) {
    $stuck = 0; $prev = $null
    for ($leg = 0; $leg -lt $maxLegs; $leg++) {
        $s = Get-State
        $d = Dist3 $s.pos_cm $target
        if ($d -le $tolCm -or $s.inside) { return $s }
        if ($null -ne $prev -and (Dist3 $s.pos_cm $prev) -lt 15) { $stuck++ } else { $stuck = 0 }
        if ($stuck -ge 3) { throw ('diver stuck swimming to {0}: pos {1} target {2} dist {3:N0} cm run_state {4} tether_distance {5:N1} m' -f $what, ($s.pos_cm -join ','), ($target -join ','), $d, $s.run_state, $s.tether_distance_m) }
        $prev = $s.pos_cm
        Look-At $s.pos_cm $target
        Hold @('W') ([Math]::Min(1.2, [Math]::Max(0.2, ($d - $tolCm) / 300)))
    }
    $s = Get-State
    throw ('did not reach {0} in {1} legs: pos {2} target {3} dist {4:N0} cm run_state {5}' -f $what, $maxLegs, ($s.pos_cm -join ','), ($target -join ','), (Dist3 $s.pos_cm $target), $s.run_state)
}
# At one stop: every piece of salvage rests (does not move in 3 s) and the nearest three can be swum to and grabbed with E.
function Floor-Stop([int]$stop) {
    $s = Drive-Now ('s' + $stop)
    Need ($s.parked_stop -eq $stop) ('could not park at stop {0}: sub_depth {1:N1} limit {2}' -f $stop, $s.sub_depth_m, $s.depth_limit_m)
    Dive
    $a0 = Get-Arena
    $n = @($a0.salvage).Count
    Need ($n -gt 0) ('stop {0}: no salvage in the world' -f $stop)
    Wait-Sec 3
    $a1 = Get-Arena
    $moved = 0
    foreach ($p in $a0.salvage) { $q = @($a1.salvage | ?{ $_.name -eq $p.name })[0]; if ($null -eq $q -or (Dist3 $p.pos_cm $q.pos_cm) -gt 5) { $moved++ } }
    $zs = @($a1.salvage | %{ [int]$_.pos_cm[2] }) | sort
    $script:Notes += ('{0} stop {1}: {2} pieces, z range {3}..{4} cm (anchor z {5:N0}), {6} moved in 3 s' -f $script:Tag, $stop, $n, $zs[0], $zs[-1], $a1.anchor_cm[2], $moved)
    Need ($moved -eq 0) ('stop {0}: {1} of {2} salvage pieces still moving 3 s after the dive (not resting on the floor)' -f $stop, $moved, $n)
    Need ($zs[-1] -lt $a1.anchor_cm[2] - 300) ('stop {0}: salvage at z {1} is not below the submarine (anchor z {2:N0})' -f $stop, $zs[-1], $a1.anchor_cm[2])
    $got = 0
    for ($k = 0; $k -lt 3; $k++) {
        $s = Get-State
        $best = $null; $bd = 1e12
        foreach ($p in (Get-Arena).salvage) { $d = Dist3 $p.pos_cm $s.pos_cm; if ($d -lt $bd) { $bd = $d; $best = $p } }
        if ($null -eq $best) { break }
        $items0 = $s.cargo_items
        $s = Swim-To $best.pos_cm 110 40 ('salvage {0} at stop {1}' -f $best.name, $stop)
        for ($t = 0; $t -lt 4 -and (Get-State).cargo_items -le $items0; $t++) { Look-At (Get-State).pos_cm $best.pos_cm; Hold @('E') 0.3; Wait-Sec 0.5 }
        $s = Get-State
        $script:Notes += ('{0} stop {1} piece {2} {3}: items {4} -> {5}, cargo {6} kg, diver {7}' -f $script:Tag, $stop, $k, $best.name, $items0, $s.cargo_items, $s.cargo_kg, ($s.pos_cm -join ','))
        Need ($s.cargo_items -gt $items0) ('stop {0}: E beside salvage {1} did not grab it (diver {2}, piece {3}, items {4})' -f $stop, $best.name, ($s.pos_cm -join ','), ($best.pos_cm -join ','), $s.cargo_items)
        $got++
    }
    $null = Run-Exec 'Deep.EnterSub'
    $null = Wait-State { param($s) $s.inside } 10 'back aboard after the floor check'
    $null = Run-Exec 'Deep.Console'
    $null = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'banking the floor haul'
    Press-Continue
}
function S9-floor {
    $script:Tag = 'S9'
    Begin-Run
    $null = Run-Exec 'Deep.BuyGate free'; $null = Run-Exec 'Deep.BuyGate free'
    foreach ($st in 1, 2, 3) { Floor-Stop $st }
    Shot 'S9-floor-end'
}
# The relic in its sinkhole at stop 3: swim to it from the hatch by keys, take it, come home, bank it, win.
function S10-relic {
    Begin-Run
    $null = Run-Exec 'Deep.BuyGate free'; $null = Run-Exec 'Deep.BuyGate free'
    $s = Drive-Now 's3'
    Need ($s.parked_stop -eq 3) ('could not park at stop 3: sub_depth {0:N1}' -f $s.sub_depth_m)
    Dive
    $s = Get-State
    Need ($s.relic_in_world) 'no relic in the world at stop 3'
    $anc = (Get-Arena).anchor_cm
    # the relic is the piece whose distance to the diver equals relic_distance_m
    $rp = $null
    foreach ($p in (Get-Arena).salvage) { if ([Math]::Abs((Dist3 $p.pos_cm $s.pos_cm) / 100 - $s.relic_distance_m) -lt 0.5) { $rp = $p } }
    Need ($null -ne $rp) ('could not identify the relic among the salvage (relic_distance {0:N1} m)' -f $s.relic_distance_m)
    $script:Notes += ('S10 relic at {0}, {1:N1} m from the diver, {2:N0} cm below the anchor, tether {3} m' -f ($rp.pos_cm -join ','), $s.relic_distance_m, ($anc[2] - $rp.pos_cm[2]), $s.tether_length_m)
    Shot 'S10-relic-dive'
    # a sinkhole has walls: swim level to above its centre first, then straight down it (a straight steep line from the hatch hits the rim)
    $above = @($rp.pos_cm[0], $rp.pos_cm[1], ($anc[2] - 700))
    $s = Swim-To $above 150 40 'the point above the sinkhole'
    $script:Notes += ('S10 above the sinkhole: diver {0} tether_distance {1:N1} m' -f ($s.pos_cm -join ','), $s.tether_distance_m)
    $s = Swim-To $rp.pos_cm 110 80 'the relic'
    for ($t = 0; $t -lt 5 -and -not (Get-State).carrying_relic; $t++) { Look-At (Get-State).pos_cm $rp.pos_cm; Hold @('E') 0.3; Wait-Sec 0.5 }
    $s = Get-State
    $script:Notes += ('S10 at the relic: carrying={0} distance {1:N1} m diver {2} tether_distance {3:N1} m' -f $s.carrying_relic, $s.relic_distance_m, ($s.pos_cm -join ','), $s.tether_distance_m)
    Shot 'S10-relic-grab'
    Need ($s.carrying_relic) ('swam to the relic but E did not take it: diver {0} relic {1} dist {2:N1} m' -f ($s.pos_cm -join ','), ($rp.pos_cm -join ','), $s.relic_distance_m)
    # home: the relic overloads the diver (60 kg), so swimming is out by design; the player winches (hold R)
    $s = Get-State
    $script:Notes += ('S10 loaded: overloaded {0} cargo_kg {1} lamp {2} (the relic is meant to be winched home; NoLamp {3})' -f $s.overloaded, $s.cargo_kg, $s.lamp, [bool]$NoLamp)
    if ($NoLamp) { $null = Run-Exec 'Deep.Lamp 0' } else { $null = Run-Exec 'Deep.Lamp 1' }
    $r = Ascend-With-Relic $anc 'S10'
    $s = Get-State
    $script:Notes += ('S10 home with the relic: inside {0} run_state {1} carrying {2} tether_distance {3:N1} m diver {4}; {5}' -f $s.inside, $s.run_state, $s.carrying_relic, $s.tether_distance_m, ($s.pos_cm -join ','), $r)
    Need $s.inside ('could not climb back aboard with the relic: diver {0} anchor {1} tether_distance {2:N1} m run_state {3}; {4}' -f ($s.pos_cm -join ','), ($anc -join ','), $s.tether_distance_m, $s.run_state, $r)
    Need $s.carrying_relic ('home, but the relic was lost on the way: {0}' -f $r)
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.won } 15 'the win after banking the relic'
    $script:Notes += ('S10 won: rating {0} kept {1}' -f $s.run_totals.rating, $s.run_totals.kept)
    Wait-Sec 1
}
# ---- the last ascent, played the way a careful player plays it ---------------------------------------------
# The lamp is the defence: when the serpent stalks, bites or hunts within the beam's reach, aim at its head and keep
# it there until it turns away (Recoil). Otherwise look ahead, level (not up the line: that would be a free defence).
$script:LampReachM = 30
function Serpent-Threatens($s) {
    if ($NoLamp -or $null -eq $s.serpent) { return $false }
    $z = $s.serpent
    if ($z.bite_alarm_s -gt 0) { return $true }
    return (($z.state -in @('Stalk', 'Bite', 'Hunt')) -and $z.diver_distance_m -le $script:LampReachM)
}
function Aim-Lamp-At-Serpent($s) {
    $eye = @($s.pos_cm[0], $s.pos_cm[1], ($s.pos_cm[2] + 60))
    Look-At $eye $s.serpent.head_cm
}
# Winch home; on a cut, harpoon the sub if it is in reach, else drop the relic and swim home on bailout air.
# Returns a summary string; the caller checks inside / carrying_relic.
function Ascend-With-Relic($anc, [string]$tag) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $bites = 0; $repels = 0; $aimS = 0.0; $lastState = ''; $events = @(); $cutAt = $null
    $prevd = (Get-State).tether_distance_m; $flatSince = $sw.Elapsed.TotalSeconds
    while ($sw.Elapsed.TotalSeconds -lt 120) {
        $s = Get-State
        if ($s.inside -or $s.run_state -in @('dead', 'aboard', 'settling', 'won')) { break }
        $z = $s.serpent
        if ($z -and $z.state -ne $lastState) {
            if ($z.state -eq 'Bite') { $bites++ }
            if ($lastState -in @('Stalk', 'Bite', 'Hunt') -and $z.state -eq 'Recoil') { $repels++ }
            $events += ('{0:N1}s {1}@{2:N0}m' -f $sw.Elapsed.TotalSeconds, $z.state, $s.tether_distance_m)
            $lastState = $z.state
        }
        if ($s.run_state -eq 'cut') {
            if ($null -eq $cutAt) {
                $cutAt = $s.tether_distance_m
                $events += ('{0:N1}s CUT diver {1:N1} m from the anchor' -f $sw.Elapsed.TotalSeconds, $cutAt)
                # the last resort: the harpoon on the sub, if it is within its reach
                if ($s.harpoon.shots -gt 0 -and $cutAt -le 25) {
                    $null = Run-Exec 'Deep.Harpoon.AimSub fire'; Wait-Sec 0.6
                    $h = (Get-State).harpoon
                    $events += ('harpoon phase {0} latch {1} last {2}' -f $h.phase, $h.latch, $h.last)
                }
            }
            $h = $s.harpoon
            if ($h.latch -eq 'sub') { Wait-Sec 0.3; continue }   # being reeled in to the hatch
            # out of harpoon reach (or it missed): the relic is lost, the diver is not. Drop it and swim up.
            if ($s.carrying_relic) {
                for ($k = 0; $k -lt 3 -and (Get-State).cargo_kg -gt 0; $k++) { Hold @('G') 0.2; Wait-Sec 0.2 }
                $events += ('{0:N1}s dropped the cargo, swimming home on {1:N0} s of air' -f $sw.Elapsed.TotalSeconds, $s.bailout_air_s)
            }
            if (Serpent-Threatens $s) { Aim-Lamp-At-Serpent $s; Hold @('SpaceBar') 0.4; $aimS += 0.4 }
            elseif ($s.pos_cm[2] -lt $anc[2] - 1500) { Look-At $s.pos_cm @($anc[0], $anc[1], ($s.pos_cm[2] + 1000)); Hold @('W', 'LeftShift') 1.0 }   # up the shaft, steering back to its axis (under the hatch) as it goes
            else { Look-At $s.pos_cm @($anc[0], $anc[1], ($anc[2] - 60)); Hold @('W', 'LeftShift') 1.0 }
            continue
        }
        # R stays held the whole way (Deep.Winch 1 is the winch key held down); the mouse aims meanwhile.
        if (-not $s.winching) { $null = Run-Exec 'Deep.Winch 1' }
        $t0 = $sw.Elapsed.TotalSeconds
        if (Serpent-Threatens $s) { Aim-Lamp-At-Serpent $s; Wait-Sec 0.1; $aimS += $sw.Elapsed.TotalSeconds - $t0 }
        else { $null = Run-Exec ('Deep.Look 0 {0:N1}' -f (Yaw-Of ($anc[0] - $s.pos_cm[0]) ($anc[1] - $s.pos_cm[1]))); Wait-Sec 0.1 }
        if ([Math]::Abs($s.tether_distance_m - $prevd) -ge 0.2) { $flatSince = $sw.Elapsed.TotalSeconds; $prevd = $s.tether_distance_m }
        if ($sw.Elapsed.TotalSeconds - $flatSince -gt 8) { throw ('{0}: winching made no progress for 8 s: diver {1} tether_distance {2:N1} m run_state {3}' -f $tag, ($s.pos_cm -join ','), $s.tether_distance_m, $s.run_state) }
    }
    $s = Get-State
    return ('{0:N0} s, bites {1}, repels {2}, lamp aimed {3:N1} s, cut at {4}, serpent: {5}' -f $sw.Elapsed.TotalSeconds, $bites, $repels, $aimS, $(if ($null -eq $cutAt) { 'never' } else { '{0:N1} m' -f $cutAt }), ($events -join ', '))
}
# A cut at the bottom of the sinkhole with the relic aboard: drop it, swim home on bailout air (lamp defence). Survives?
function S10-cutdrop {
    Begin-Run
    $null = Run-Exec 'Deep.BuyGate free'; $null = Run-Exec 'Deep.BuyGate free'
    $s = Drive-Now 's3'
    Need ($s.parked_stop -eq 3) ('could not park at stop 3: sub_depth {0:N1}' -f $s.sub_depth_m)
    Dive
    $anc = (Get-Arena).anchor_cm
    $null = Run-Exec 'Deep.Relic'; Wait-Sec 0.5
    $null = Run-Exec 'Deep.Grab'; Wait-Sec 0.5
    $s = Get-State
    Need ($s.carrying_relic) ('the relic was not picked up (distance {0:N1} m)' -f $s.relic_distance_m)
    $null = Run-Exec 'Deep.Harpoon.Reset'
    $null = Run-Exec 'Deep.ForceCut'; Wait-Sec 0.3
    $s = Get-State
    $script:Notes += ('S10-cutdrop cut at {0:N1} m from the anchor, air {1:N0} s, cargo {2} kg, diver {3}' -f $s.tether_distance_m, $s.bailout_air_s, $s.cargo_kg, ($s.pos_cm -join ','))
    Need ($s.run_state -eq 'cut') ('ForceCut did not cut: run_state {0}' -f $s.run_state)
    $r = Ascend-With-Relic $anc 'S10-cutdrop'
    $s = Get-State
    $script:Notes += ('S10-cutdrop end: inside {0} run_state {1} diver_number {2}; {3}' -f $s.inside, $s.run_state, $s.diver_number, $r)
    Need ($s.inside -and $s.diver_number -eq 1) ('the cut diver who dropped the relic did not get home: run_state {0} diver_number {1}; {2}' -f $s.run_state, $s.diver_number, $r)
}
# The serpent's first pass: staged once per run for a player-controlled diver; it must end, and then normal hunting follows.
function S11-pass {
    Begin-Run
    $null = Run-Exec 'Deep.BuyGate free'
    $s = Drive-Now 's2'
    Need ($s.parked_stop -eq 2) ('could not park at stop 2: sub_depth {0:N1}' -f $s.sub_depth_m)
    Dive
    $m = Get-Monster
    Need ($null -ne $m) 'could not read DEEPMONSTER'
    $script:Notes += ('S11 on arrival: present={0} state={1} first_sight_done={2}' -f $m.present, $m.state, $m.first_sight_done)
    Need (-not $m.first_sight_done) 'the first sight was already used before the diver did anything'
    # swim and winch: noise. Watch for the Pass state, then for it to end.
    $states = @(); $sawPass = $false; $sw = [Diagnostics.Stopwatch]::StartNew(); $passEnded = $false
    while ($sw.Elapsed.TotalSeconds -lt 100) {
        $s = Get-State
        if ($s.run_state -ne 'tethered') { break }
        Hold @('W', 'LeftShift') 1.0
        $m = Get-Monster
        if ($null -eq $m) { continue }
        if ($states.Count -eq 0 -or $states[-1] -ne $m.state) { $states += $m.state }
        if ($m.state -eq 'Pass') { $sawPass = $true }
        if ($sawPass -and $m.state -ne 'Pass') { $passEnded = $true; break }
        if ($m.first_sight_done -and $m.state -ne 'Pass') { $passEnded = $true; break }
        $s = Get-State
        # back under the hatch before the line runs out (a diver pinned at the end of the line is still and silent)
        if ($s.tether_distance_m -gt 30) { $anc = (Get-Arena).anchor_cm; Teleport @($anc[0], $anc[1], ($anc[2] - 800)) }
    }
    $script:Notes += ('S11 states seen after {0:N0} s: {1}; saw Pass {2}; first_sight_done {3}' -f $sw.Elapsed.TotalSeconds, ($states -join ' > '), $sawPass, $m.first_sight_done)
    Shot 'S11-pass'
    Need $sawPass 'no first pass within 100 s of noisy swimming at stop 2'
    Need $passEnded ('the pass did not end: state {0}' -f $m.state)
    Need $m.first_sight_done 'first_sight_done is false after the pass ended'
    # Normal hunting: with the first sight used, a sprinting diver is heard and stalked as before.
    # Sprint noise only counts above the sprint threshold (5.2 m/s at the default 4 m/s x 1.6) and gains 15/s
    # against a 10/s decay, so it takes about 12 s of unbroken sprinting to reach 60. Short holds from rest never
    # reach the threshold, and a diver pinned at the end of the 60 m line is still: both are silent. So the diver
    # sprints 5 s legs round a 32 m square under the hatch in open water, re-aimed at the next corner between holds
    # (each MCP round trip leaves the keys up for about a second, so the legs must be long).
    # Patience is topped up and the lamp is off so that only hearing can bring the serpent in (a Captain's cut or
    # a lamp recoil must not count, or hide a stalk).
    $s = Get-State
    Need ($s.run_state -eq 'tethered') ('the pass cut the run short: run_state {0}' -f $s.run_state)
    $null = Run-Exec 'Deep.SetPatience 1'
    $null = Run-Exec 'Deep.Lamp 0'
    $m0 = Get-Monster
    $anc = (Get-Arena).anchor_cm
    $z = $anc[2] - 800
    $corners = @(@(($anc[0] - 1600), ($anc[1] - 1600), $z), @(($anc[0] + 1600), ($anc[1] - 1600), $z),
                 @(($anc[0] + 1600), ($anc[1] + 1600), $z), @(($anc[0] - 1600), ($anc[1] + 1600), $z))
    Teleport $corners[0]
    $s = Get-State
    $leg = 5.0
    $c = 0; $hunted = $false; $why = ''; $seen = @(); $noiseMax = 0.0; $speeds = @()
    $lap = [Diagnostics.Stopwatch]::StartNew(); $prev = $s.pos_cm
    while ($lap.Elapsed.TotalSeconds -lt 75 -and -not $hunted) {
        # aim at the next corner from wherever the diver is (Look-At also returns no state: one call per leg is the cost)
        $c = ($c + 1) % 4
        Look-At $s.pos_cm $corners[$c]
        Hold @('W', 'LeftShift') $leg
        # every Exec returns the state report; this one changes nothing (the lamp is already off)
        $s = Run-Exec 'Deep.Lamp 0'
        $ddx = $s.pos_cm[0] - $prev[0]; $ddy = $s.pos_cm[1] - $prev[1]; $ddz = $s.pos_cm[2] - $prev[2]
        $speeds += ([Math]::Sqrt($ddx * $ddx + $ddy * $ddy + $ddz * $ddz) / 100) / $leg
        $prev = $s.pos_cm
        if ($null -ne $s.serpent) {
            if ($seen.Count -eq 0 -or $seen[-1] -ne $s.serpent.state) { $seen += $s.serpent.state }
            $noiseMax = [Math]::Max($noiseMax, [double]$s.serpent.noise)
            if ($s.serpent.state -in @('Stalk', 'Bite')) { $hunted = $true; $why = $s.serpent.state }
        }
        if ($s.last_event -in @('bite_alarm', 'cut_bite')) { $hunted = $true; $why = $s.last_event }
        if ($s.run_state -ne 'tethered') { break }
    }
    $m = Get-Monster
    $null = Run-Exec 'Deep.Lamp 1'
    # metres per held second, legs after the first (which starts from rest)
    $cruise = @($speeds | Select-Object -Skip 1)
    $mean = if ($cruise.Count -gt 0) { ($cruise | Measure-Object -Average).Average } else { 0 }
    $heard = if ($m -and $m0) { [double]$m.heard - [double]$m0.heard } else { -1 }
    $script:Notes += ('S11 after the pass: {0:N0} s of sprint legs, diver m per held s mean {1:N1} (min {2:N1}); serpent {3}; noise max {4:N1}; heard +{5:N1}; hunted {6} ({7}); run_state {8} last_event {9}' -f
        $lap.Elapsed.TotalSeconds, $mean, ($cruise | Measure-Object -Minimum).Minimum, ($seen -join ' > '), $noiseMax, $heard, $hunted, $why, $s.run_state, $s.last_event)
    Need ($s.run_state -eq 'tethered' -or $hunted) ('the run left the tether for another reason before the serpent hunted: run_state {0}, last_event {1}' -f $s.run_state, $s.last_event)
    Need ($mean -ge 5.2) ('the scripted diver did not sprint (mean {0:N1} m/s, threshold 5.2): the script is at fault, not the serpent' -f $mean)
    Need ($heard -gt 0) ('the serpent heard nothing from a diver sprinting at {0:N1} m/s' -f $mean)
    Need $hunted ('after the pass the serpent never stalked a sprinting diver: state {0}, noise max {1:N1}, heard +{2:N1}' -f $m.state, $noiseMax, $heard)
    # and the run continues: the diver can still get home (winch)
    $null = Run-Exec 'Deep.Monster.State'
    $null = Run-Exec 'Deep.EnterSub'
    $s = Wait-State { param($s) $s.inside } 15 'getting home after the first pass'
    Need ($s.inside) 'could not get home after the pass'
}
# A diver returning from 15 to 20 m out to the side of the submarine: swim straight at it, and separately winch.
function S12-sidehome {
    Begin-Run
    Dive
    $anc = (Get-Arena).anchor_cm
    $cases = @(
        @{ n = 'swim +side 18 m'; dx = 0; dy = 1800; how = 'swim' },
        @{ n = 'swim -side 15 m'; dx = 0; dy = -1500; how = 'swim' },
        @{ n = 'swim +side 20 m'; dx = 0; dy = 2000; how = 'swim' },
        @{ n = 'winch -side 18 m'; dx = 0; dy = -1800; how = 'winch' },
        @{ n = 'winch +side 20 m'; dx = 0; dy = 2000; how = 'winch' }
    )
    foreach ($c in $cases) {
        $s = Get-State
        if ($s.inside) { $null = Dive }
        for ($i = 0; $i -lt 4; $i++) { Teleport @(($anc[0] + $c.dx), ($anc[1] + $c.dy), ($anc[2] - 300)); if ((Get-State).tether_distance_m -gt 12) { break } }
        $s = Get-State
        $d0 = $s.tether_distance_m
        $script:Notes += ('S12 {0}: start {1} tether_distance {2:N1} m run_state {3}' -f $c.n, ($s.pos_cm -join ','), $d0, $s.run_state)
        Need ($d0 -gt 12) ('{0}: teleport did not put the diver out to the side (distance {1:N1} m)' -f $c.n, $d0)
        $sw = [Diagnostics.Stopwatch]::StartNew()
        if ($c.how -eq 'swim') {
            for ($leg = 0; $leg -lt 60; $leg++) {
                $s = Get-State
                if ($s.inside) { break }
                Look-At $s.pos_cm @($anc[0], $anc[1], ($anc[2] - 60))
                Hold @('W') 1.0
            }
        } else {
            for ($leg = 0; $leg -lt 40; $leg++) {
                if ((Get-State).inside) { break }
                Hold @('R') 1.5
            }
        }
        $s = Get-State
        $script:Notes += ("S12 $($c.n): inside $($s.inside) run_state $($s.run_state) tether_distance $([Math]::Round($s.tether_distance_m,1)) m diver $($s.pos_cm -join ',') after $([int]$sw.Elapsed.TotalSeconds) s")
        Need $s.inside ("$($c.n): the diver did not get home. diver $($s.pos_cm -join ',') anchor $($anc -join ',') tether_distance $([Math]::Round($s.tether_distance_m,1)) m (from $([Math]::Round($d0,1))) run_state $($s.run_state)")
    }
}
# ---- driver ----------------------------------------------------------------------------------
$list = @($Scenario)
if ($Scenario -eq 'all') { $list = $All }
foreach ($n in $list) { if ($All -notcontains $n) { Write-Host ('unknown scenario ' + $n + '; known: ' + ($All -join ' ')); exit 2 } }

$total = [Diagnostics.Stopwatch]::StartNew()
$startedEditor = $false
if (-not (Wait-Editor 3)) {
    Write-Host ('editor not answering on {0}; starting it' -f $Port)
    $startedEditor = $true
    # Agent editors run in the background; Windows-side the editor throttles itself then. Turn that off for this editor only.
    $extra = ''
    if (-not $NoUnthrottle) { $extra = '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False' }
    $null = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Tools 'ue.ps1') editor -Port $Port
    if (-not (Wait-Editor 240)) { Write-Host 'FAIL all: editor never answered'; exit 1 }
    # no fixed wait: Start-Pie below retries while the editor still runs its start-up slow task
}
$null = Mcp 'call' 'EditorToolset.EditorAppToolset.StopPIE' '{}'

$pass = 0; $fail = 0
foreach ($n in $list) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $script:Notes = @()
    $err = $null
    try {
        for ($tries = 0; $tries -lt 151; $tries++) {
            try { Start-Pie; break } catch { if ($_.Exception.Message -match 'slow task|not ready|busy' -and $tries -lt 150) { Start-Sleep -Seconds 1 } else { throw } }
        }
        & $n
    } catch { $err = $_.Exception.Message -replace '\s+', ' '; if ($env:PT_TRACE) { $err += ' @ ' + ($_.ScriptStackTrace -replace '\s+', ' ') } }
    try { Shot $n } catch { }
    try { $z = Get-State; Write-Host ('  END {0}: run_state={1} stop={2} parked={3} sub_depth={4:N0} credits={5} cargo_kg={6} hull={7} flashlight={8} won={9}' -f $n, $z.run_state, $z.stop, $z.parked_stop, $z.sub_depth_m, $z.credits, $z.cargo_kg, $z.hull_rating, $z.has_flashlight, $z.won) } catch { }
    try { Stop-Pie } catch { }
    foreach ($x in $script:Notes) { Write-Host ('  ' + $x) }
    if ($err) { $fail++; Write-Host ('FAIL {0}: {1} ({2:N0}s)' -f $n, $err, $sw.Elapsed.TotalSeconds) }
    else { $pass++; Write-Host ('PASS {0} ({1:N0}s)' -f $n, $sw.Elapsed.TotalSeconds) }
}
Write-Host ('TOTAL {0} passed, {1} failed in {2:N0}s; screenshots in {3} and {4}' -f $pass, $fail, $total.Elapsed.TotalSeconds, $ShotDir, $MediaDir)
if ($startedEditor -and -not $KeepEditor) { $null = Mcp 'call' 'EditorToolset.EditorAppToolset.ExitEditor' '{}' }
if ($fail -eq 0) { exit 0 } else { exit 1 }










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
  S7-run      full run: stop 1, flashlight, stop 2, hull upgrade, stop 3, relic, win screen
Starts the editor if the port does not answer, runs PIE per scenario, asserts on DeepToolset.GetState JSON
(and the DEEPMONSTER log line for the serpent), saves a viewport PNG per scenario to
deep\Saved\Harness\playtest\<scenario>.png and devlog\media\2026-11-15-play-<scenario>.png, stops PIE and prints
PASS or FAIL <scenario>: <reason>. Exit code 0 only if everything passed.
Keys: W/A/S/D move, Space up/jump, C down, E use/grab, G drop, R winch, F lamp, LMB harpoon (ADeepPlayerController).
#>
param(
    [Parameter(Position = 0)][string]$Scenario = 'all',
    [int]$Port = 8012,
    [switch]$KeepEditor
)
$ErrorActionPreference = 'Stop'
$Tools = $PSScriptRoot
$Repo = Split-Path -Parent $PSScriptRoot
$ShotDir = Join-Path $Repo 'deep\Saved\Harness\playtest'
$MediaDir = Join-Path $Repo 'devlog\media'
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
New-Item -ItemType Directory -Force -Path $MediaDir | Out-Null
$All = @('S0-title', 'S1-hatch', 'S2-gate', 'S3-serpent', 'S4-cut', 'S5-harpoon', 'S6-bank', 'S7-run')
$script:Notes = @()

function Mcp([string]$verb, [string]$name, [string]$json) {
    $sq = [string][char]39
    $dq = [string][char]34
    $cmd = '& ' + $sq + (Join-Path $Tools 'mcp.ps1') + $sq + ' ' + $verb + ' ' + $sq + $name + $sq
    if ($json) { $cmd += ' ' + $sq + $json.Replace($sq, $sq + $sq) + $sq }
    $cmd += ' -Port ' + $Port + ' -MaxChars 100000'
    $a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', ($dq + $cmd.Replace($dq, '\' + $dq) + $dq))
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $out = & powershell.exe @a 2>&1 | Out-String
    $code = $LASTEXITCODE
    $ErrorActionPreference = $old
    return [pscustomobject]@{ Ok = ($code -eq 0); Text = $out.Trim() }
}
function Call([string]$tool, [string]$json) {
    $r = Mcp 'call' $tool $json
    if (-not $r.Ok) { throw ('{0} failed: {1}' -f $tool, $r.Text) }
    return $r
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
    $j = '{"entries":[' + ($e -join ',') + '],"durationSeconds":' + $sec.ToString([Globalization.CultureInfo]::InvariantCulture) + '}'
    $null = Call 'InputToolset.InputToolset.ApplyInput' $j
}
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([int]($s * 1000)) }
function Need([bool]$cond, [string]$why) { if (-not $cond) { throw $why } }
function Teleport($p) { $null = Run-Exec ('Deep.Teleport {0} {1} {2}' -f [int]$p[0], [int]$p[1], [int]$p[2]); Wait-Sec 0.5 }
function Near([double]$a, [double]$b, [double]$tol) { return ([Math]::Abs($a - $b) -le $tol) }

function Wait-Editor([int]$timeoutSec) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $timeoutSec) {
        $r = Mcp 'ping' '' ''
        if ($r.Ok -and $r.Text -match '^OK') { return $true }
        Start-Sleep -Seconds 3
    }
    return $false
}

function Start-Pie {
    $null = Call 'EditorToolset.EditorAppToolset.StartPIE' '{}'
    for ($i = 0; $i -lt 20; $i++) {
        Wait-Sec 1
        $r = Mcp 'call' 'DeepToolset.DeepToolset.GetState' '{}'
        if ($r.Ok) {
            $s = ($r.Text | ConvertFrom-Json).returnValue | ConvertFrom-Json
            if ($s.diver_present) { Wait-Sec 1.5; return }
        }
    }
    throw 'PIE started but no diver appeared within 20 s'
}
function Stop-Pie {
    $null = Mcp 'call' 'EditorToolset.EditorAppToolset.StopPIE' '{}'
    Wait-Sec 2
}
# One viewport PNG: kept in the harness folder and in devlog\media (named for the dev log).
function Shot([string]$name) {
    $r = Mcp 'call' 'SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport' '{"world":null,"maxResolution":1280}'
    if ($r.Ok -and $r.Text -match 'image saved: (.+\.png)') {
        $src = $Matches[1].Trim()
        Copy-Item -LiteralPath $src -Destination (Join-Path $ShotDir ($name + '.png')) -Force
        Copy-Item -LiteralPath $src -Destination (Join-Path $MediaDir ('2026-11-15-play-' + $name + '.png')) -Force
    } else { $script:Notes += ($name + ': screenshot failed: ' + $r.Text) }
}

# Poll GetState until $test returns true or $timeoutSec real seconds pass; returns the last state.
function Wait-State([scriptblock]$test, [double]$timeoutSec, [string]$what) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $s = Get-State
        if (& $test $s) { return $s }
        if ($sw.Elapsed.TotalSeconds -gt $timeoutSec) { throw ('timed out after {0:N0} s waiting for {1}; run_state={2} credits={3} stop={4} sub_depth={5} inside={6}' -f $sw.Elapsed.TotalSeconds, $what, $s.run_state, $s.credits, $s.stop, $s.sub_depth_m, $s.inside) }
        Wait-Sec 0.7
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
        Wait-Sec 1
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
    $null = Run-Exec 'Deep.EnterSub'
    $null = Run-Exec 'Deep.Console'
    $s = Wait-State { param($s) $s.won } 15 'the win'
    $script:Notes += ('S7 won: rating {0} kept {1} divers {2} at {3:N0}s' -f $s.run_totals.rating, $s.run_totals.kept, $s.run_totals.divers_expended, $sw.Elapsed.TotalSeconds)
    Need ($s.won) 'bank of the relic did not win'
    Need ($null -ne $s.run_totals) 'no run totals on the win screen'
    Wait-Sec 1
}

# ---- driver ----------------------------------------------------------------------------------
$list = @($Scenario)
if ($Scenario -eq 'all') { $list = $All }
foreach ($n in $list) { if ($All -notcontains $n) { Write-Host ('unknown scenario ' + $n + '; known: ' + ($All -join ' ')); exit 2 } }

$total = [Diagnostics.Stopwatch]::StartNew()
$startedEditor = $false
if (-not (Wait-Editor 3)) {
    Write-Host ('editor not answering on {0}; starting it' -f $Port)
    $null = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Tools 'ue.ps1') editor -Port $Port
    $startedEditor = $true
    if (-not (Wait-Editor 240)) { Write-Host 'FAIL all: editor never answered'; exit 1 }
    Start-Sleep -Seconds 40   # the first PIE call is refused while the editor still runs its start-up slow task
}
$null = Mcp 'call' 'EditorToolset.EditorAppToolset.StopPIE' '{}'
Wait-Sec 1

$pass = 0; $fail = 0
foreach ($n in $list) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $script:Notes = @()
    $err = $null
    try {
        for ($tries = 0; $tries -lt 6; $tries++) {
            try { Start-Pie; break } catch { if ($_.Exception.Message -match 'slow task' -and $tries -lt 5) { Start-Sleep -Seconds 15 } else { throw } }
        }
        & $n
    } catch { $err = $_.Exception.Message -replace '\s+', ' ' }
    try { Shot $n } catch { }
    try { Stop-Pie } catch { }
    foreach ($x in $script:Notes) { Write-Host ('  ' + $x) }
    if ($err) { $fail++; Write-Host ('FAIL {0}: {1} ({2:N0}s)' -f $n, $err, $sw.Elapsed.TotalSeconds) }
    else { $pass++; Write-Host ('PASS {0} ({1:N0}s)' -f $n, $sw.Elapsed.TotalSeconds) }
}
Write-Host ('TOTAL {0} passed, {1} failed in {2:N0}s; screenshots in {3} and {4}' -f $pass, $fail, $total.Elapsed.TotalSeconds, $ShotDir, $MediaDir)
if ($startedEditor -and -not $KeepEditor) { $null = Mcp 'call' 'EditorToolset.EditorAppToolset.ExitEditor' '{}' }
if ($fail -eq 0) { exit 0 } else { exit 1 }

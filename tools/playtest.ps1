<#
playtest.ps1 - scripted playtests for DEEP through the editor's MCP server. Costs no model tokens.

  playtest.ps1 <scenario|all> [-Port 8012] [-KeepEditor]

Scenarios: S0-boot S1-grab S2-overload S3-tether S4-winch S5-cut S6-bank S7-descend S8-run
Starts the editor if the port does not answer, runs PIE per scenario, asserts on DeepToolset.GetState JSON,
saves a viewport PNG per scenario to deep\Saved\Harness\playtest\<scenario>.png, stops PIE and prints
PASS or FAIL <scenario>: <reason>. Exit code 0 only if everything passed.
Keys: W/A/S/D swim, Space up, C down, E grab, G drop, R winch, F lamp (ADeepPlayerController).
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
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
$All = @('S0-boot', 'S1-grab', 'S2-overload', 'S3-tether', 'S4-winch', 'S5-cut', 'S6-bank', 'S7-descend', 'S8-run')
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
# keys: array of key names, held together for $sec seconds
function Hold([string[]]$keys, [double]$sec) {
    $e = @(); foreach ($k in $keys) { $e += ('{"key":{"keyName":"' + $k + '"},"value":{"x":1,"y":0,"z":0}}') }
    $j = '{"entries":[' + ($e -join ',') + '],"durationSeconds":' + $sec.ToString([Globalization.CultureInfo]::InvariantCulture) + '}'
    $null = Call 'InputToolset.InputToolset.ApplyInput' $j
}
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([int]($s * 1000)) }
function Need([bool]$cond, [string]$why) { if (-not $cond) { throw $why } }
function Teleport($p) { $null = Run-Exec ('Deep.Teleport {0} {1} {2}' -f [int]$p[0], [int]$p[1], [int]$p[2]); Wait-Sec 0.5 }

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
function Shot([string]$name) {
    $r = Mcp 'call' 'SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport' '{"world":null,"maxResolution":1280}'
    if ($r.Ok -and $r.Text -match 'image saved: (.+\.png)') {
        Copy-Item -LiteralPath $Matches[1].Trim() -Destination (Join-Path $ShotDir ($name + '.png')) -Force
    } else { $script:Notes += ($name + ': screenshot failed: ' + $r.Text) }
}

# ---- scenarios -------------------------------------------------------------------------------
function S0-boot {
    $s = Get-State; $a = Get-Arena
    Need ($null -ne $a.submarine) 'no submarine in the PIE world'
    Need (@($a.salvage).Count -ge 1) 'no salvage piece in the PIE world'
    Need ($s.diver_present) 'no diver'
    Need ($s.stop -eq 1) ('stop is {0}, expected 1' -f $s.stop)
    Need ($s.tether_length_m -gt 0) 'tether length is 0'
    Need ($s.tether_distance_m -le ($s.tether_length_m + 1)) ('diver beyond tether at boot: {0} of {1} m' -f $s.tether_distance_m, $s.tether_length_m)
    $script:Notes += ('S0 salvage={0} tether={1}m dist={2:N1}m depth={3}m' -f @($a.salvage).Count, $s.tether_length_m, $s.tether_distance_m, $s.depth_m)
}
function S1-grab {
    $a = Get-Arena
    $p = $a.salvage[0].pos_cm
    Teleport @(($p[0] - 80), $p[1], $p[2])
    $s = Get-State
    Need ($s.cargo_kg -eq 0) 'cargo not empty before grab'
    $n0 = $s.salvage_in_world
    Hold @('E') 0.4
    Wait-Sec 0.5
    $s = Get-State
    $script:Notes += ('S1 cargo_kg={0} items={1} salvage {2}->{3}' -f $s.cargo_kg, $s.cargo_items, $n0, $s.salvage_in_world)
    Need ($s.cargo_kg -gt 0) ('cargo stayed {0} kg after pressing E beside a salvage piece (salvage_in_world {1}->{2})' -f $s.cargo_kg, $n0, $s.salvage_in_world)
}
function S2-overload {
    $null = Run-Exec 'Deep.GiveSalvage 30 300'
    $s0 = Get-State
    Wait-Sec 3
    $s1 = Get-State
    $script:Notes += ('S2 cargo={0}kg overloaded={1} depth {2} -> {3}' -f $s1.cargo_kg, $s1.overloaded, $s0.depth_m, $s1.depth_m)
    Need ($s1.cargo_kg -ge 30) ('cargo is {0} kg after GiveSalvage 30' -f $s1.cargo_kg)
    Need ($s1.depth_m -gt ($s0.depth_m + 0.05)) ('depth did not increase: {0} -> {1} (overloaded={2})' -f $s0.depth_m, $s1.depth_m, $s1.overloaded)
}
function S3-tether {
    $max = 0.0; $len = 0.0
    $t = [Diagnostics.Stopwatch]::StartNew()
    while ($t.Elapsed.TotalSeconds -lt 15) {
        Hold @('W', 'SpaceBar', 'LeftShift') 3
        $s = Get-State
        $len = $s.tether_length_m
        if ($s.tether_distance_m -gt $max) { $max = $s.tether_distance_m }
    }
    $script:Notes += ('S3 max distance {0:N1} m of {1} m tether, taut={2}' -f $max, $len, $s.tether_taut)
    Need ($max -gt 12) ('diver barely moved: max distance {0:N1} m, the input may not be reaching the game' -f $max)
    Need ($max -le ($len + 1.0)) ('tether broke: distance {0:N1} m > length {1} m + 1' -f $max, $len)
}
function S4-winch {
    $a = Get-Arena
    $anc = $a.anchor_cm
    # from the SIDE at anchor height (this stalled 7.4 m out before the winch fix)
    Teleport @(($anc[0] + 2500), $anc[1], $anc[2])
    $s = Get-State
    Need ($s.tether_distance_m -gt 20) ('teleport failed to get far out: {0:N1} m' -f $s.tether_distance_m)
    $d0 = $s.tether_distance_m
    Hold @('R') 8
    $s = Get-State
    $held = 8
    # The editor throttles an unfocused PIE, so game time runs slower than real time; keep winching up to 24 s total.
    while ($s.tether_distance_m -ge 5 -and $held -lt 24) { Hold @('R') 8; $held += 8; $s = Get-State }
    $script:Notes += ('S4 distance {0:N1} -> {1:N1} m after {2} s of R, home={3}, pos_cm={4}' -f $d0, $s.tether_distance_m, $held, $s.home, ($s.pos_cm -join ','))
    Need ($s.tether_distance_m -lt 5) ('after {0} s of winch the diver is still {1:N1} m from the anchor (started {2:N1})' -f $held, $s.tether_distance_m, $d0)
}

# ---- loop scenarios ----------------------------------------------------------------------------
# Poll GetState until $test returns true or $timeoutSec real seconds pass; returns the last state.
function Wait-State([scriptblock]$test, [double]$timeoutSec, [string]$what) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $s = Get-State
        if (& $test $s) { return $s }
        if ($sw.Elapsed.TotalSeconds -gt $timeoutSec) { throw ('timed out after {0:N0} s waiting for {1}; run_state={2} credits={3} stop={4}' -f $sw.Elapsed.TotalSeconds, $what, $s.run_state, $s.credits, $s.stop) }
        Wait-Sec 0.7
    }
}
# Leave the settlement screen the way a player does (Space), retrying until the minimum screen time has passed.
function Press-Continue([string]$want) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 40) {
        $s = Get-State
        if ($s.run_state -ne 'settling') { break }
        Hold @('SpaceBar') 0.3
        Wait-Sec 0.5
    }
    # the 2.5 s descent can finish between two polls, so any state other than settling counts
    $null = Wait-State { param($s) $s.run_state -ne 'settling' } 40 'the settlement screen to close'
}
# Teleport next to / away from the airlock and check it took; Deep.Teleport was seen to land at the wrong place now and then.
function Far-Out {
    $a = Get-Arena; $anc = $a.anchor_cm
    for ($i = 0; $i -lt 4; $i++) {
        Teleport @(($anc[0] + 2500), $anc[1], $anc[2])
        if ((Get-State).tether_distance_m -gt 15) { return $anc }
        $script:Notes += 'teleport far out did not take, retrying'
    }
    throw 'Deep.Teleport never put the diver 25 m from the airlock'
}
function Go-Home($anc) {
    for ($i = 0; $i -lt 4; $i++) {
        Teleport @(($anc[0] + 300), $anc[1], $anc[2])
        Wait-Sec 0.5
        $s = Get-State
        if ($s.tether_distance_m -lt 6 -or $s.run_state -ne 'cut') { return }
        $script:Notes += ('teleport home left the diver {0:N1} m out, retrying' -f $s.tether_distance_m)
    }
}
function Cut-Of([int]$g) { return [int][Math]::Ceiling($g * 0.4) }

function S5-cut {
    # earn a few credits so the fees are visible: gross 100 -> net 45 (no hull yet)
    $null = Run-Exec 'Deep.GiveSalvage 10 100'
    $null = Run-Exec 'Deep.Bank'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling'
    Need ($s.credits -eq 45) ('credits {0} after banking 100, expected 45' -f $s.credits)
    Press-Continue 'tethered'
    $c0 = (Get-State).credits
    # branch A: Patience to near zero far from the airlock, then reach the airlock in time
    $anc = Far-Out
    $null = Run-Exec 'Deep.SetPatience 0.02'
    $s = Wait-State { param($s) $s.run_state -eq 'cut' } 30 'Patience to cut the tether'
    $a0 = $s.bailout_air_s
    Need ($a0 -gt 0) 'bailout air is 0 right after the cut'
    Wait-Sec 3
    $s = Get-State
    $script:Notes += ('S5 cut: air {0:N1} -> {1:N1} s, state {2}' -f $a0, $s.bailout_air_s, $s.run_state)
    Need ($s.bailout_air_s -lt $a0) ('bailout air did not count down: {0} -> {1}' -f $a0, $s.bailout_air_s)
    Shot 'S5-cut-bailout'
    Go-Home $anc
    $s = Wait-State { param($s) $s.run_state -ne 'cut' } 20 're-tether at the airlock'
    $script:Notes += ('S5 retether: state {0} credits {1} -> {2}' -f $s.run_state, $c0, $s.credits)
    Need ($s.run_state -eq 'tethered') ('after re-tether run_state is ' + $s.run_state)
    Need ($s.credits -eq [Math]::Max(0, $c0 - 20)) ('credits {0}, expected {1} after the 20 re-tether fee' -f $s.credits, ($c0 - 20))
    # branch B: cut again far out, with cargo, and let the air run out
    $n0 = $s.diver_number; $cb = $s.credits
    $null = Far-Out
    $null = Run-Exec 'Deep.GiveSalvage 5 50'
    $null = Run-Exec 'Deep.ForceCut'
    $s = Wait-State { param($s) $s.run_state -eq 'cut' } 10 'forced cut'
    $script:Notes += ('S5 B: cut at {0:N1} m from the airlock, air {1:N1} s, credits {2}, diver {3}' -f $s.tether_distance_m, $s.bailout_air_s, $s.credits, $n0)
    # the 3 s death screen can pass between two polls, so accept "dead" or an already-respawned diver
    $s = Wait-State { param($s) $s.run_state -eq 'dead' -or $s.diver_number -gt $n0 } 100 'bailout air to run out (death)'
    $script:Notes += ('S5 death: diver {0} -> {1} credits {2} -> {3} state {4}' -f $n0, $s.diver_number, $cb, $s.credits, $s.run_state)
    Need ($s.diver_number -gt $n0 -or $s.run_state -eq 'dead') 'no death'
    Need ($s.credits -eq [Math]::Max(0, $cb - 30)) ('credits {0} after death, expected {1} (30 fee)' -f $s.credits, [Math]::Max(0, $cb - 30))
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 30 'respawn'
    Need ($s.diver_number -eq ($n0 + 1)) ('diver number {0}, expected {1}' -f $s.diver_number, ($n0 + 1))
    Need ($s.cargo_kg -eq 0) ('cargo is {0} kg after respawn' -f $s.cargo_kg)
    Need ($s.tether_distance_m -lt 15) ('respawned {0:N1} m from the airlock' -f $s.tether_distance_m)
}
function S6-bank {
    $null = Run-Exec 'Deep.ResetRun'
    # A: tiny haul where fees exceed the take: net must clamp at zero, credits stay >= 0
    $null = Run-Exec 'Deep.GiveSalvage 1 5'
    $null = Run-Exec 'Deep.Bank'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling (small bank)'
    $script:Notes += ('S6 small: gross {0} cut {1} fees {2} net {3} credits {4}' -f $s.last_bank.gross, $s.last_bank.cut, $s.last_bank.fees, $s.last_bank.net, $s.credits)
    Need ($s.last_bank.gross -eq 5) ('gross {0}, expected 5' -f $s.last_bank.gross)
    Need ($s.last_bank.net -ge 0) ('net went negative: {0}' -f $s.last_bank.net)
    Need ($s.credits -eq [Math]::Max(0, $s.last_bank.net)) ('credits {0} after a net of {1}' -f $s.credits, $s.last_bank.net)
    Press-Continue 'tethered'
    # B: gross 100: cut = 40, fees 10 percent + 5 = 15, net 45 (below the 50 hull price)
    $c0 = (Get-State).credits
    $null = Run-Exec 'Deep.GiveSalvage 10 100'
    $null = Run-Exec 'Deep.Bank'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling'
    $b = $s.last_bank
    $script:Notes += ('S6 bank: gross {0} cut {1} fees {2} net {3} credits {4} -> {5}' -f $b.gross, $b.cut, $b.fees, $b.net, $c0, $s.credits)
    Need ($b.gross -eq 100) ('gross {0}, expected 100' -f $b.gross)
    Need ($b.cut -eq (Cut-Of 100)) ('cut {0}, expected {1} (40 percent rounded up)' -f $b.cut, (Cut-Of 100))
    Need ($b.fees -eq 15) ('fees {0}, expected 15 (10 percent + 5)' -f $b.fees)
    $net = [Math]::Max(0, $b.gross - $b.cut - $b.fees)
    Need ($b.net -eq $net) ('net {0}, expected {1}' -f $b.net, $net)
    Need ($s.credits -eq ($c0 + $net)) ('credits {0}, expected {1} + {2}' -f $s.credits, $c0, $net)
    Need ($s.cargo_kg -eq 0) 'cargo not emptied by the bank'
    # left on the settlement screen for the screenshot
}
function S7-descend {
    $null = Run-Exec 'Deep.ResetRun'
    $s0 = Get-State
    Need ($s0.stop -eq 1 -and $s0.hull_rating -eq 1) ('start state stop {0} hull {1}' -f $s0.stop, $s0.hull_rating)
    # gross 300: cut 120, fees 35, net 145; hull rating 2 costs 50
    $null = Run-Exec 'Deep.GiveSalvage 20 300'
    $null = Run-Exec 'Deep.Bank'
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 10 'settling'
    $script:Notes += ('S7 bank: net {0} hull_bought={1} hull {2} credits {3}' -f $s.last_bank.net, $s.last_bank.hull_bought, $s.hull_rating, $s.credits)
    Need ($s.last_bank.hull_bought) 'hull was not bought with 145 net credits'
    Need ($s.hull_rating -eq 2) ('hull rating {0}, expected 2' -f $s.hull_rating)
    Need ($s.credits -eq ($s.last_bank.net - 50)) ('credits {0}, expected net {1} minus the 50 hull price' -f $s.credits, $s.last_bank.net)
    Press-Continue 'descending'
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 40 'arrival at the new stop'
    $script:Notes += ('S7 stop {0} depth {1} m salvage {2} diver {3:N1} m from the airlock' -f $s.stop, $s.depth_m, $s.salvage_in_world, $s.tether_distance_m)
    Need ($s.stop -eq 2) ('stop is {0}, expected 2' -f $s.stop)
    Need ([Math]::Abs($s.depth_m - 300) -lt 40) ('depth {0} m, expected about 300' -f $s.depth_m)
    Need ($s.salvage_in_world -ge 1) 'no salvage at stop 2'
    Need ($s.cargo_kg -eq 0) 'cargo not empty at the new stop'
}
function S8-run {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $null = Run-Exec 'Deep.ResetRun'
    Wait-Sec 1.5
    # swim out like a player
    $p0 = (Get-State).pos_cm
    Hold @('W', 'SpaceBar', 'LeftShift') 3
    $s = Get-State
    $moved = [Math]::Sqrt([Math]::Pow($s.pos_cm[0] - $p0[0], 2) + [Math]::Pow($s.pos_cm[1] - $p0[1], 2) + [Math]::Pow($s.pos_cm[2] - $p0[2], 2)) / 100
    Need ($moved -gt 2) ('input did not move the diver: only {0:N1} m in 3 s of W + Space + Shift' -f $moved)
    # grab a real piece of salvage
    # (the salvage ring is re-spawned by ResetRun, so ask the arena again on every try)
    for ($try = 0; $try -lt 3 -and (Get-State).cargo_kg -eq 0; $try++) {
        $a = Get-Arena
        $p = $a.salvage[0].pos_cm
        Teleport @(($p[0] - 80), $p[1], $p[2])
        Hold @('E') 0.4
        Wait-Sec 0.7
    }
    $s = Get-State
    Need ($s.cargo_kg -gt 0) ('grab failed in the run after 3 tries (salvage in world {0}, {1:N1} m from the airlock)' -f $s.salvage_in_world, $s.tether_distance_m)
    # top the haul up so the hull can be bought, then winch home from the side
    $null = Run-Exec 'Deep.GiveSalvage 20 300'
    $null = Far-Out
    Hold @('R') 8
    $s = Get-State; $held = 8
    while ($s.run_state -eq 'tethered' -and $held -lt 40) { Hold @('R') 6; $held += 6; $s = Get-State }
    $s = Wait-State { param($s) $s.run_state -eq 'settling' } 15 'auto-bank on arrival'
    $script:Notes += ('S8 banked: net {0} hull {1} credits {2} at {3:N0}s' -f $s.last_bank.net, $s.hull_rating, $s.credits, $sw.Elapsed.TotalSeconds)
    Need ($s.hull_rating -eq 2) ('hull rating {0} after the bank' -f $s.hull_rating)
    Press-Continue 'descending'
    $s = Wait-State { param($s) $s.run_state -eq 'tethered' } 40 'arrival at stop 2'
    $script:Notes += ('S8 stop {0} depth {1} m in {2:N0}s' -f $s.stop, $s.depth_m, $sw.Elapsed.TotalSeconds)
    Need ($s.stop -eq 2) ('stop is {0}, expected 2' -f $s.stop)
    Need ($sw.Elapsed.TotalSeconds -lt 180) ('run took {0:N0} s, over 3 minutes' -f $sw.Elapsed.TotalSeconds)
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
}
$null = Mcp 'call' 'EditorToolset.EditorAppToolset.StopPIE' '{}'
Wait-Sec 1

$pass = 0; $fail = 0
foreach ($n in $list) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $script:Notes = @()
    $err = $null
    try {
        Start-Pie
        & $n
    } catch { $err = $_.Exception.Message -replace '\s+', ' ' }
    try { Shot $n } catch { }
    try { Stop-Pie } catch { }
    foreach ($x in $script:Notes) { Write-Host ('  ' + $x) }
    if ($err) { $fail++; Write-Host ('FAIL {0}: {1} ({2:N0}s)' -f $n, $err, $sw.Elapsed.TotalSeconds) }
    else { $pass++; Write-Host ('PASS {0} ({1:N0}s)' -f $n, $sw.Elapsed.TotalSeconds) }
}
Write-Host ('TOTAL {0} passed, {1} failed in {2:N0}s; screenshots in {3}' -f $pass, $fail, $total.Elapsed.TotalSeconds, $ShotDir)
if ($startedEditor -and -not $KeepEditor) { $null = Mcp 'call' 'EditorToolset.EditorAppToolset.ExitEditor' '{}' }
if ($fail -eq 0) { exit 0 } else { exit 1 }

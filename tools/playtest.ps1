<#
playtest.ps1 - scripted playtests for DEEP through the editor's MCP server. Costs no model tokens.

  playtest.ps1 <scenario|all> [-Port 8012] [-KeepEditor]

Scenarios: S0-boot S1-grab S2-overload S3-tether S4-winch
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
$All = @('S0-boot', 'S1-grab', 'S2-overload', 'S3-tether', 'S4-winch')
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
    Teleport @(($anc[0] + 2500), $anc[1], ($anc[2] - 1500))
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

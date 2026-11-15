<#
ue.ps1 - DEEP build / test / package harness. Windows PowerShell 5.1. Keep this file pure ASCII.

Usage:  .\tools\ue.ps1 <verb> [-Filter Deep] [-SmokeSeconds 25]
        .\tools\ue.ps1 shot -Name <name> [-Delay 6] [-Cmds "cmd1;cmd2"]
Verbs:  info | build | test | verify | package | zip | smoke | shot | editor | clean
Exit code 0 means PASS. Anything else is FAIL. Full logs go to deep\Saved\Harness\.
Engine root: $env:DEEP_UE_ROOT if set, else C:\Program Files\Epic Games\UE_5.9.
If scripts are blocked: powershell -ExecutionPolicy Bypass -File tools\ue.ps1 <verb>
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('info', 'build', 'test', 'verify', 'package', 'zip', 'smoke', 'shot', 'editor', 'clean', 'help')]
    [string]$Verb = 'help',
    [string]$Filter = 'Deep',
    [int]$SmokeSeconds = 25,
    [string]$Name = 'shot',
    [int]$Delay = 6,
    [string]$Cmds = ''
)

$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------- paths
$RepoRoot   = Split-Path -Parent $PSScriptRoot
$ProjectDir = Join-Path $RepoRoot 'deep'
$UProject   = Join-Path $ProjectDir 'Deep.uproject'
$BuildsDir  = Join-Path $RepoRoot 'Builds'
$JamJson    = Join-Path $PSScriptRoot 'jam.json'
$HarnessDir = Join-Path $ProjectDir 'Saved\Harness'
if ($env:DEEP_UE_ROOT) { $EngineRoot = $env:DEEP_UE_ROOT.TrimEnd('\') } else { $EngineRoot = 'C:\Program Files\Epic Games\UE_5.9' }
$BuildBat  = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$RunUAT    = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'

$TimeoutBuildMin   = 20
$TimeoutTestMin    = 15
$TimeoutPackageMin = 60
$TimeoutZipMin     = 15
$McpPortEditor     = 8000
$McpPortHeadless   = 8011   # every editor process auto-starts MCP; keep headless runs off 8000
$SizeLimitMB       = 1024

# ---------------------------------------------------------------- helpers
function New-Stamp { return (Get-Date -Format 'yyyyMMdd-HHmmss') }

function Initialize-HarnessDir {
    if (-not (Test-Path -LiteralPath $HarnessDir)) { $null = New-Item -ItemType Directory -Force -Path $HarnessDir }
}

function Format-Dur([TimeSpan]$t) {
    if ($t.TotalSeconds -lt 90) { return ('{0:N1}s' -f $t.TotalSeconds) }
    return ('{0}m{1:00}s' -f [int][math]::Floor($t.TotalMinutes), $t.Seconds)
}

function Write-Result([bool]$Pass, [string]$Name, [TimeSpan]$Dur, [string]$Note) {
    if ($Pass) { $word = 'PASS' } else { $word = 'FAIL' }
    $line = '{0} {1} ({2})' -f $word, $Name, (Format-Dur $Dur)
    if ($Note) { $line = $line + ' - ' + $Note }
    Write-Host $line
}

function Stop-Tree([int]$ProcId) {
    # cmd swallows taskkill output so PowerShell never sees native stderr
    & cmd.exe /d /c "taskkill /PID $ProcId /T /F >nul 2>&1"
}

# Start a process with a verbatim argument string, stdout to a file, hard timeout, kill tree on timeout.
function Invoke-Proc([string]$File, [string]$Arguments, [string]$StdOut, [int]$TimeoutSec) {
    $errFile = $StdOut + '.stderr'
    $r = New-Object psobject -Property @{ ExitCode = -1; TimedOut = $false; Started = $false; Error = '' }
    try {
        $p = Start-Process -FilePath $File -ArgumentList $Arguments -PassThru -NoNewWindow `
            -RedirectStandardOutput $StdOut -RedirectStandardError $errFile -ErrorAction Stop
    } catch {
        $r.Error = $_.Exception.Message
        return $r
    }
    $r.Started = $true
    $null = $p.Handle   # keep the handle so ExitCode is readable later
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        $r.TimedOut = $true
        Stop-Tree $p.Id
        $null = $p.WaitForExit(15000)
    } else {
        $r.ExitCode = $p.ExitCode
    }
    # Fold stderr into the main log. Helper processes may hold the handle briefly, so retry.
    $merged = $false
    for ($i = 0; $i -lt 5 -and (Test-Path -LiteralPath $errFile); $i++) {
        try {
            if (-not $merged) {
                $e = Get-Content -LiteralPath $errFile -Raw -ErrorAction Stop
                if ($e -and $e.Trim()) { Add-Content -LiteralPath $StdOut -Value ("`r`n----- stderr -----`r`n" + $e) -ErrorAction Stop }
                $merged = $true
            }
            Remove-Item -LiteralPath $errFile -Force -ErrorAction Stop
        } catch { Start-Sleep -Milliseconds 400 }
    }
    return $r
}

# Unique matching lines from a log, trimmed. Emits strings; wrap calls in @().
function Get-MatchLines([string]$Path, [string]$Pattern, [int]$Max) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return }
    $seen = @{}
    $n = 0
    foreach ($m in (Select-String -LiteralPath $Path -Pattern $Pattern)) {
        $t = $m.Line.Trim()
        if ($t.Length -gt 300) { $t = $t.Substring(0, 300) + '...' }
        if ($seen.ContainsKey($t)) { continue }
        $seen[$t] = $true
        $t
        $n++
        if ($n -ge $Max) { break }
    }
}

function Get-TailLines([string]$Path, [int]$Count) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return }
    Get-Content -LiteralPath $Path -Tail $Count | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ }
}

function Write-Indented($Lines) {
    foreach ($l in $Lines) { Write-Host ('  ' + $l) }
}

# Editor processes whose command line names this project.
function Get-ProjectEditors([string[]]$Names) {
    if (-not $Names) { $Names = @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') }
    $needle = $UProject.ToLower()
    $procs = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" -ErrorAction SilentlyContinue)
    foreach ($p in $procs) {
        if ($Names -notcontains $p.Name) { continue }
        $cl = [string]$p.CommandLine
        if (-not $cl) {
            Write-Host ('  warn: {0} pid {1} command line unreadable (elevated?); cannot tell which project it has open' -f $p.Name, $p.ProcessId)
            continue
        }
        $c = $cl.ToLower().Replace('/', '\')
        if ($c.Contains($needle) -or $c -match 'deep\.uproject') { $p }
    }
}

function Test-PortListening([int]$Port) {
    try {
        $c = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction Stop)
        return ($c.Count -gt 0)
    } catch { return $false }
}

function Find-PackagedDir {
    $d = Join-Path $BuildsDir 'Windows'
    if (Test-Path -LiteralPath (Join-Path $d 'Deep.exe')) { return $d }
    if (-not (Test-Path -LiteralPath $BuildsDir)) { return $null }
    $exe = Get-ChildItem -LiteralPath $BuildsDir -Filter 'Deep.exe' -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\Binaries\\' } | Sort-Object { $_.FullName.Length } | Select-Object -First 1
    if ($exe) { return $exe.DirectoryName }
    return $null
}

function Get-DirSizeMB([string]$Dir) {
    $s = (Get-ChildItem -LiteralPath $Dir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    if (-not $s) { $s = 0 }
    return [math]::Round($s / 1MB, 1)
}

function Test-EngineOk([string]$Name, [System.Diagnostics.Stopwatch]$Sw, [string]$Need) {
    if (Test-Path -LiteralPath $Need) { return $true }
    Write-Result $false $Name $Sw.Elapsed ('engine file not found: ' + $Need + ' (set DEEP_UE_ROOT?)')
    return $false
}

function Write-BusyEditors($Busy) {
    foreach ($b in $Busy) { Write-Host ('  running: {0} pid {1}' -f $b.Name, $b.ProcessId) }
}

# ---------------------------------------------------------------- verbs
$BuildErrPattern = '(?i)(\)\s*:\s*(fatal\s+)?error\b|:\s*error\s+(C|LNK|MSB)\d+|^\s*ERROR\s*:|\berror\s*:|Live Coding is active|Unable to build while|Result:\s*Failed)'

function Invoke-Build {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not (Test-EngineOk 'build' $sw $BuildBat)) { return 1 }
    $busy = @(Get-ProjectEditors)
    if ($busy.Count -gt 0) {
        Write-Result $false 'build' $sw.Elapsed 'an editor is running on this project'
        Write-BusyEditors $busy
        Write-Host '  It locks UnrealEditor-Deep.dll. Compile through Live Coding in the editor (Ctrl+Alt+F11 or LiveCodingToolset), or close the editor and re-run.'
        return 1
    }
    Initialize-HarnessDir
    $log = Join-Path $HarnessDir ('build-{0}.log' -f (New-Stamp))
    $argLine = '/d /s /c ""{0}" DeepEditor Win64 Development -Project="{1}" -WaitMutex"' -f $BuildBat, $UProject
    Write-Host ('build: DeepEditor Win64 Development, timeout {0} min' -f $TimeoutBuildMin)
    $r = Invoke-Proc 'cmd.exe' $argLine $log ($TimeoutBuildMin * 60)
    if (-not $r.Started) {
        Write-Result $false 'build' $sw.Elapsed ('could not start: ' + $r.Error)
        return 1
    }
    if ($r.TimedOut) {
        Write-Result $false 'build' $sw.Elapsed ('timed out after {0} min; process tree killed' -f $TimeoutBuildMin)
        Write-Indented @(Get-TailLines $log 10)
        Write-Host ('  log: ' + $log)
        return 1
    }
    if ($r.ExitCode -ne 0) {
        Write-Result $false 'build' $sw.Elapsed ('exit code {0}' -f $r.ExitCode)
        $errs = @(Get-MatchLines $log $BuildErrPattern 20)
        if ($errs.Count -eq 0) { $errs = @(Get-TailLines $log 15) }
        Write-Indented $errs
        Write-Host ('  log: ' + $log)
        return 1
    }
    $summary = @(Get-MatchLines $log '(?i)(Target is up to date|Total execution time|Result:\s*Succeeded)' 2)
    Write-Result $true 'build' $sw.Elapsed ''
    Write-Indented $summary
    Write-Host ('  log: ' + $log)
    return 0
}

function Invoke-Test {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not (Test-EngineOk 'test' $sw $EditorCmd)) { return 1 }
    Initialize-HarnessDir
    $stamp = New-Stamp
    $log = Join-Path $HarnessDir ('test-{0}.log' -f $stamp)
    $out = Join-Path $HarnessDir ('test-{0}.stdout.log' -f $stamp)
    $reportDir = Join-Path $HarnessDir ('test-{0}-report' -f $stamp)
    $index = Join-Path $reportDir 'index.json'
    # Unreal's RunTests filter is a plain substring match ("Deep" also hits engine tests named "...deep...").
    # A bare filter is anchored to the start of the test path with ^. Prefix with * for a substring match.
    # ^X, X$, StartsWith:X, Group:X and A+B are passed through unchanged.
    if ($Filter.StartsWith('*')) { $ueFilter = $Filter.Substring(1) }
    elseif ($Filter -match '^(\^|StartsWith:|Group:)' -or $Filter.Contains('+')) { $ueFilter = $Filter }
    else { $ueFilter = '^' + $Filter }
    $argLine = ('"{0}" -ExecCmds="Automation RunTests {1}; Quit" -TestExit="Automation Test Queue Empty" ' +
        '-ReportExportPath="{2}" -unattended -nopause -nosplash -nullrhi -NoSound -log -abslog="{3}" ' +
        '-ModelContextProtocolPort={4}') -f $UProject, $ueFilter, $reportDir, $log, $McpPortHeadless
    Write-Host ("test: Automation RunTests {0} (headless, timeout {1} min)" -f $ueFilter, $TimeoutTestMin)
    $r = Invoke-Proc $EditorCmd $argLine $out ($TimeoutTestMin * 60)
    if (-not (Test-Path -LiteralPath $log)) { $log = $out }   # editor died before opening its log
    $crash = @(Get-MatchLines $log '(Fatal error|Unhandled Exception|Assertion failed)' 5)

    if (-not $r.Started) {
        Write-Result $false 'test' $sw.Elapsed ('could not start: ' + $r.Error)
        return 1
    }
    if ($r.TimedOut) {
        Write-Result $false 'test' $sw.Elapsed ('timed out after {0} min; process tree killed' -f $TimeoutTestMin)
        Write-Indented @(Get-TailLines $log 10)
        Write-Host ('  log: ' + $log)
        return 1
    }
    if (-not (Test-Path -LiteralPath $index)) {
        Write-Result $false 'test' $sw.Elapsed ('no report written (crash, or no test matched "{0}"); exit code {1}' -f $Filter, $r.ExitCode)
        $lines = $crash
        if ($lines.Count -eq 0) {
            $lines = @(Get-MatchLines $log ('(?i)(Message dialog closed|could not be (successfully )?(loaded|initialized|compiled)|' +
                'Log\w+: Error:|LogAutomation\w*:.*(Error|Warning|No automation|not found|0 tests))') 10)
        }
        if ($lines.Count -eq 0) { $lines = @(Get-TailLines $log 10) }
        Write-Indented $lines
        Write-Host ('  log: ' + $log)
        return 1
    }
    try {
        $json = Get-Content -LiteralPath $index -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    } catch {
        Write-Result $false 'test' $sw.Elapsed ('report unreadable: ' + $_.Exception.Message)
        Write-Host ('  report: ' + $index)
        return 1
    }
    if ($json.tests) { $tests = @($json.tests) } else { $tests = @() }
    $bad = @($tests | Where-Object { $_.state -ne 'Success' })
    $total = $tests.Count
    $passed = $total - $bad.Count

    $reasons = @()
    if ($total -eq 0) { $reasons += ('zero tests ran for filter "{0}"' -f $Filter) }
    if ($bad.Count -gt 0) { $reasons += ('{0} of {1} failed' -f $bad.Count, $total) }
    if ($crash.Count -gt 0) { $reasons += 'crash marker in log' }
    if ($r.ExitCode -ne 0) { $reasons += ('editor exit code {0}' -f $r.ExitCode) }
    $pass = ($reasons.Count -eq 0)

    if ($pass) { $note = ('{0} tests passed' -f $total) } else { $note = ($reasons -join '; ') }
    Write-Result $pass 'test' $sw.Elapsed $note
    Write-Host ('  tests: {0} run, {1} passed, {2} failed, filter "{3}"' -f $total, $passed, $bad.Count, $Filter)
    $shown = 0
    foreach ($t in $bad) {
        if ($shown -ge 8) { Write-Host ('  ... and {0} more failing tests (see report)' -f ($bad.Count - $shown)); break }
        $shown++
        Write-Host ('  FAIL {0} [{1}]' -f $t.fullTestPath, $t.state)
        $msgs = @()
        foreach ($en in @($t.entries)) {
            if (-not $en -or -not $en.event) { continue }
            if ($en.event.type -eq 'Error' -or $en.event.type -eq 'Warning') {
                $m = '{0}: {1}' -f $en.event.type, $en.event.message
                if ($en.filename) { $m = $m + (' ({0}:{1})' -f (Split-Path -Leaf $en.filename), $en.lineNumber) }
                $msgs += $m
            }
        }
        if ($msgs.Count -eq 0) { $msgs = @('(no error messages recorded)') }
        foreach ($m in ($msgs | Select-Object -First 4)) {
            if ($m.Length -gt 300) { $m = $m.Substring(0, 300) + '...' }
            Write-Host ('      ' + $m)
        }
    }
    Write-Indented $crash
    Write-Host ('  report: ' + $index)
    Write-Host ('  log: ' + $log)
    if ($pass) { Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue }
    if ($pass) { return 0 } else { return 1 }
}

function Invoke-Verify {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $code = Invoke-Build
    if ($code -eq 0) { $code = Invoke-Test }
    Write-Result ($code -eq 0) 'verify' $sw.Elapsed ''
    return $code
}

function Invoke-Package {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not (Test-EngineOk 'package' $sw $RunUAT)) { return 1 }
    $busy = @(Get-ProjectEditors @('UnrealEditor.exe'))
    if ($busy.Count -gt 0) {
        Write-Result $false 'package' $sw.Elapsed 'an editor is running on this project'
        Write-BusyEditors $busy
        Write-Host '  BuildCookRun also builds the editor target. Close the editor and re-run.'
        return 1
    }
    Initialize-HarnessDir
    $log = Join-Path $HarnessDir ('package-{0}.log' -f (New-Stamp))
    $argLine = ('/d /s /c ""{0}" BuildCookRun -project="{1}" -noP4 -platform=Win64 -clientconfig=Development ' +
        '-build -cook -stage -pak -archive -archivedirectory="{2}" -prereqs -nodebuginfo -utf8output -unattended"') -f $RunUAT, $UProject, $BuildsDir
    Write-Host ('package: BuildCookRun Win64 Development to {0}, timeout {1} min' -f $BuildsDir, $TimeoutPackageMin)
    $r = Invoke-Proc 'cmd.exe' $argLine $log ($TimeoutPackageMin * 60)
    if (-not $r.Started) {
        Write-Result $false 'package' $sw.Elapsed ('could not start: ' + $r.Error)
        return 1
    }
    if ($r.TimedOut) {
        Write-Result $false 'package' $sw.Elapsed ('timed out after {0} min; process tree killed' -f $TimeoutPackageMin)
        Write-Indented @(Get-TailLines $log 10)
        Write-Host ('  log: ' + $log)
        return 1
    }
    if ($r.ExitCode -ne 0) {
        Write-Result $false 'package' $sw.Elapsed ('exit code {0}' -f $r.ExitCode)
        $errs = @(Get-MatchLines $log ('(?i)(\)\s*:\s*(fatal\s+)?error\b|:\s*error\s+(C|LNK|MSB)\d+|^\s*ERROR\s*:|\bError:\s|' +
            'ExitCode=[1-9]\d*|BUILD FAILED|Cook failed)') 20)
        if ($errs.Count -eq 0) { $errs = @(Get-TailLines $log 15) }
        Write-Indented $errs
        Write-Host ('  log: ' + $log)
        return 1
    }
    $dir = Find-PackagedDir
    if (-not $dir) {
        Write-Result $false 'package' $sw.Elapsed ('UAT exited 0 but no Deep.exe found under ' + $BuildsDir)
        Write-Host ('  log: ' + $log)
        return 1
    }
    $mb = Get-DirSizeMB $dir
    Write-Result $true 'package' $sw.Elapsed ('{0} MB' -f $mb)
    Write-Host ('  exe: ' + (Join-Path $dir 'Deep.exe'))
    Write-Host ('  size: {0} MB in {1}' -f $mb, $dir)
    if ($mb -ge $SizeLimitMB) { Write-Host ('  WARN: build is {0} MB, at or over the 1 GB jam limit' -f $mb) }
    Write-Host ('  log: ' + $log)
    return 0
}

function Get-JamNames {
    if (-not (Test-Path -LiteralPath $JamJson)) {
        Set-Content -LiteralPath $JamJson -Value '{"teamName":"TeamName","gameName":"DEEP"}' -Encoding ASCII
        Write-Host ('  created ' + $JamJson)
    }
    return (Get-Content -LiteralPath $JamJson -Raw | ConvertFrom-Json)
}

function Invoke-Zip {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $dir = Find-PackagedDir
    if (-not $dir) {
        Write-Result $false 'zip' $sw.Elapsed ('no packaged build under {0}; run package first' -f $BuildsDir)
        return 1
    }
    try { $j = Get-JamNames } catch {
        Write-Result $false 'zip' $sw.Elapsed ('cannot read ' + $JamJson + ': ' + $_.Exception.Message)
        return 1
    }
    $team = ([string]$j.teamName) -replace '[^A-Za-z0-9_-]', ''
    $game = ([string]$j.gameName) -replace '[^A-Za-z0-9_-]', ''
    if (-not $team -or -not $game) {
        Write-Result $false 'zip' $sw.Elapsed ('teamName and gameName must be set in ' + $JamJson)
        return 1
    }
    $zip = Join-Path $BuildsDir ('{0}_{1}.zip' -f $team, $game)
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Initialize-HarnessDir
    $log = Join-Path $HarnessDir ('zip-{0}.log' -f (New-Stamp))
    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    $argLine = '-a -c -f "{0}" -C "{1}" "{2}"' -f $zip, (Split-Path -Parent $dir), (Split-Path -Leaf $dir)
    $r = Invoke-Proc $tar $argLine $log ($TimeoutZipMin * 60)
    if (-not $r.Started -or $r.TimedOut -or $r.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $zip)) {
        Write-Result $false 'zip' $sw.Elapsed ('tar failed: exit {0}, timedOut {1} {2}' -f $r.ExitCode, $r.TimedOut, $r.Error)
        Write-Indented @(Get-TailLines $log 10)
        Write-Host ('  log: ' + $log)
        return 1
    }
    $mb = [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)
    Write-Result $true 'zip' $sw.Elapsed ('{0} MB' -f $mb)
    Write-Host ('  zip: ' + $zip)
    if ($team -eq 'TeamName') { Write-Host ('  WARN: teamName is still the placeholder "TeamName"; edit ' + $JamJson) }
    if ($mb -ge $SizeLimitMB) { Write-Host ('  WARN: zip is {0} MB, at or over the 1 GB jam limit' -f $mb) }
    return 0
}

function Invoke-Smoke {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $dir = Find-PackagedDir
    if (-not $dir) {
        Write-Result $false 'smoke' $sw.Elapsed ('no packaged build under {0}; run package first' -f $BuildsDir)
        return 1
    }
    $exe = Join-Path $dir 'Deep.exe'
    Initialize-HarnessDir
    $log = Join-Path $HarnessDir ('smoke-{0}.log' -f (New-Stamp))
    $argLine = '-windowed -ResX=1280 -ResY=720 -log -abslog="{0}"' -f $log
    Write-Host ('smoke: running {0} for {1}s' -f $exe, $SmokeSeconds)
    try {
        $p = Start-Process -FilePath $exe -ArgumentList $argLine -PassThru -ErrorAction Stop
    } catch {
        Write-Result $false 'smoke' $sw.Elapsed ('could not start: ' + $_.Exception.Message)
        return 1
    }
    $null = $p.Handle
    $exitedEarly = $p.WaitForExit($SmokeSeconds * 1000)
    $early = ''
    if ($exitedEarly) {
        $early = 'game exited after {0:N0}s with code {1}' -f $sw.Elapsed.TotalSeconds, $p.ExitCode
    } else {
        Stop-Tree $p.Id
        $null = $p.WaitForExit(10000)
    }
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path -LiteralPath $log)) {
        Write-Result $false 'smoke' $sw.Elapsed ('no log written. ' + $early)
        return 1
    }
    $hits = @(Get-MatchLines $log '(Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception)' 10)
    $reasons = @()
    if ($hits.Count -gt 0) { $reasons += ('{0} crash/ensure lines' -f $hits.Count) }
    if ($early) { $reasons += $early }
    $pass = ($reasons.Count -eq 0)
    if ($pass) { $note = ('ran {0}s clean' -f $SmokeSeconds) } else { $note = ($reasons -join '; ') }
    Write-Result $pass 'smoke' $sw.Elapsed $note
    Write-Indented $hits
    Write-Host ('  log: ' + $log)
    if ($pass) { return 0 } else { return 1 }
}

function Invoke-Shot {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not (Test-EngineOk 'shot' $sw $EditorExe)) { return 1 }
    $busy = @(Get-ProjectEditors @('UnrealEditor.exe'))
    if ($busy.Count -gt 0) {
        Write-Result $false 'shot' $sw.Elapsed 'an editor/game is running on this project'
        Write-BusyEditors $busy
        return 1
    }
    $safe = $Name -replace '[^A-Za-z0-9_-]', ''
    if (-not $safe) { $safe = 'shot' }
    $shotDir = Join-Path $HarnessDir 'shots'
    if (-not (Test-Path -LiteralPath $shotDir)) { $null = New-Item -ItemType Directory -Force -Path $shotDir }
    $stamp = New-Stamp
    $png = Join-Path $shotDir ('{0}-{1}.png' -f $safe, $stamp)
    $log = Join-Path $HarnessDir ('shot-{0}.log' -f $stamp)
    $argLine = ('"{0}" -game -windowed -ResX=1280 -ResY=720 -nosplash -unattended -log -abslog="{1}" ' +
        '-ModelContextProtocolPort={2} -DeepShot="{3}" -DeepShotDelay={4} -DeepShotCmds="{5}"') -f $UProject, $log, $McpPortHeadless, $png, $Delay, $Cmds
    Write-Host ('shot: {0} (delay {1}s, timeout 120s)' -f $safe, $Delay)
    try {
        $p = Start-Process -FilePath $EditorExe -ArgumentList $argLine -PassThru -ErrorAction Stop
    } catch {
        Write-Result $false 'shot' $sw.Elapsed ('could not start: ' + $_.Exception.Message)
        return 1
    }
    $null = $p.Handle
    $found = $false
    while ($sw.Elapsed.TotalSeconds -lt 120) {
        if (Test-Path -LiteralPath $png) {
            Start-Sleep -Milliseconds 1500   # let the writer finish
            $found = $true
            break
        }
        if ($p.HasExited) { Start-Sleep -Milliseconds 800; if (Test-Path -LiteralPath $png) { $found = $true }; break }
        Start-Sleep -Milliseconds 500
    }
    if (-not $p.HasExited) {
        Stop-Tree $p.Id
        $null = $p.WaitForExit(10000)
    }
    if (-not $found -or -not (Test-Path -LiteralPath $png) -or (Get-Item -LiteralPath $png).Length -eq 0) {
        Write-Result $false 'shot' $sw.Elapsed 'no screenshot produced; game-side -DeepShot support may be missing'
        Write-Host ('  expected: ' + $png)
        Write-Host ('  log: ' + $log)
        return 1
    }
    Write-Result $true 'shot' $sw.Elapsed ''
    Write-Host ('  png: ' + $png)
    Write-Host ('  log: ' + $log)
    return 0
}

function Invoke-Editor {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not (Test-EngineOk 'editor' $sw $EditorExe)) { return 1 }
    $url = 'http://127.0.0.1:{0}/mcp' -f $McpPortEditor
    $busy = @(Get-ProjectEditors @('UnrealEditor.exe'))
    if ($busy.Count -gt 0) {
        Write-Result $true 'editor' $sw.Elapsed 'already running on this project'
        Write-BusyEditors $busy
        Write-Host ('  MCP: ' + $url)
        return 0
    }
    if (Test-PortListening $McpPortEditor) { Write-Host ('  WARN: port {0} is already listening; MCP may fail to bind' -f $McpPortEditor) }
    $argLine = '"{0}" -ModelContextProtocolStartServer -ModelContextProtocolPort={1}' -f $UProject, $McpPortEditor
    try {
        $p = Start-Process -FilePath $EditorExe -ArgumentList $argLine -PassThru -ErrorAction Stop
    } catch {
        Write-Result $false 'editor' $sw.Elapsed ('could not start: ' + $_.Exception.Message)
        return 1
    }
    Write-Result $true 'editor' $sw.Elapsed ('started pid {0}' -f $p.Id)
    Write-Host ('  MCP: {0} (answers once the editor has finished loading)' -f $url)
    return 0
}

function Invoke-Info {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $ver = 'unknown'
    $vf = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (Test-Path -LiteralPath $vf) {
        try {
            $v = Get-Content -LiteralPath $vf -Raw | ConvertFrom-Json
            $ver = '{0}.{1}.{2} CL {3} ({4})' -f $v.MajorVersion, $v.MinorVersion, $v.PatchVersion, $v.Changelist, $v.BranchName
        } catch { }
    }
    $engineOk = Test-Path -LiteralPath $EditorCmd
    $projOk = Test-Path -LiteralPath $UProject
    $busy = @(Get-ProjectEditors)
    $port = Test-PortListening $McpPortEditor
    Write-Result ($engineOk -and $projOk) 'info' $sw.Elapsed ''
    Write-Host ('  engine:  {0} [{1}]' -f $EngineRoot, $ver)
    if (-not $engineOk) { Write-Host ('  MISSING: ' + $EditorCmd) }
    Write-Host ('  project: {0} [{1}]' -f $UProject, $(if ($projOk) { 'found' } else { 'MISSING' }))
    if ($busy.Count -gt 0) { Write-BusyEditors $busy } else { Write-Host '  editor:  not running on this project' }
    Write-Host ('  port {0}: {1}' -f $McpPortEditor, $(if ($port) { 'listening (MCP http://127.0.0.1:8000/mcp)' } else { 'not listening' }))
    $pk = Find-PackagedDir
    if ($pk) { Write-Host ('  package: {0} ({1} MB)' -f $pk, (Get-DirSizeMB $pk)) } else { Write-Host '  package: none' }
    Write-Host ('  logs:    ' + $HarnessDir)
    if ($engineOk -and $projOk) { return 0 } else { return 1 }
}

function Invoke-Clean {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $busy = @(Get-ProjectEditors)
    if ($busy.Count -gt 0) {
        Write-Result $false 'clean' $sw.Elapsed 'an editor is running on this project; close it first'
        Write-BusyEditors $busy
        return 1
    }
    $failed = @()
    foreach ($name in @('Binaries', 'Intermediate')) {
        $d = Join-Path $ProjectDir $name
        if (Test-Path -LiteralPath $d) {
            try { Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction Stop } catch { $failed += ($d + ': ' + $_.Exception.Message) }
        }
    }
    if ($failed.Count -gt 0) {
        Write-Result $false 'clean' $sw.Elapsed 'could not remove everything'
        Write-Indented $failed
        return 1
    }
    Write-Result $true 'clean' $sw.Elapsed 'removed deep\Binaries and deep\Intermediate'
    return 0
}

function Show-Help {
    Write-Host 'usage: .\tools\ue.ps1 <verb> [-Filter Deep] [-SmokeSeconds 25]'
    Write-Host '  info     engine, project, editor running?, port 8000 listening?'
    Write-Host '  build    UBT DeepEditor Win64 Development (editor must be closed)'
    Write-Host '  test     headless Automation RunTests <Filter> (default Deep), reads index.json'
    Write-Host '  verify   build then test'
    Write-Host '  package  RunUAT BuildCookRun Win64 Development to Builds\'
    Write-Host '  zip      Builds\<team>_<game>.zip from tools\jam.json'
    Write-Host '  smoke    run the packaged exe for N seconds, scan the log'
    Write-Host '  shot     -Name n [-Delay 6] [-Cmds "a;b"] launch game, wait for -DeepShot png'
    Write-Host '  editor   start the interactive editor with MCP on port 8000'
    Write-Host '  clean    delete deep\Binaries and deep\Intermediate'
    return 0
}

# ---------------------------------------------------------------- main
$code = 1
switch ($Verb) {
    'info'    { $code = Invoke-Info }
    'build'   { $code = Invoke-Build }
    'test'    { $code = Invoke-Test }
    'verify'  { $code = Invoke-Verify }
    'package' { $code = Invoke-Package }
    'zip'     { $code = Invoke-Zip }
    'smoke'   { $code = Invoke-Smoke }
    'shot'    { $code = Invoke-Shot }
    'editor'  { $code = Invoke-Editor }
    'clean'   { $code = Invoke-Clean }
    default   { $code = Show-Help }
}
exit $code

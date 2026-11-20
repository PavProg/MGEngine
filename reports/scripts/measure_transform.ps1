# Capture and compare TransformSystem on the same generated scene.
[CmdletBinding()]
param(
    [string]$ExeDir = (Join-Path $PSScriptRoot '..\..\build\windows-x64-profile'),
    [string]$TracyDir = (Join-Path $PSScriptRoot '..\..\..\windows-0.14.1'),
    [string]$OutDir = (Join-Path $PSScriptRoot ('..\..\build\transform-bench\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [ValidateRange(1, 30)][int]$Runs = 3,
    [ValidateSet('serial', 'parallel')][string[]]$Modes = @('serial', 'parallel')
)

$ErrorActionPreference = 'Stop'
$ExeDir = (Resolve-Path -LiteralPath $ExeDir).Path
$TracyDir = (Resolve-Path -LiteralPath $TracyDir).Path
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
$exe = Join-Path $ExeDir 'MyGameEngine.exe'
$captureExe = Join-Path $TracyDir 'tracy-capture.exe'
$csvExport = Join-Path $TracyDir 'tracy-csvexport.exe'
foreach ($expected in @($exe, $captureExe, $csvExport)) {
    if (-not (Test-Path -LiteralPath $expected -PathType Leaf)) { throw "File not found: $expected" }
}

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -LiteralPath $OutDir -Force).Count) {
    throw "Choose an empty output directory: $OutDir"
}
$traces = Join-Path $OutDir 'traces'
$logs = Join-Path $OutDir 'logs'
[void][IO.Directory]::CreateDirectory($traces)
[void][IO.Directory]::CreateDirectory($logs)

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$scene = Join-Path $repo 'assets\bench\transform_scene.json'
& (Join-Path $repo 'tools\gen_transform_bench_scene.ps1') -OutFile $scene
$targetScene = Join-Path $ExeDir 'assets\bench\transform_scene.json'
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($targetScene))
if ($scene -ne $targetScene) { Copy-Item -LiteralPath $scene -Destination $targetScene }
$sceneHash = (Get-FileHash -LiteralPath $scene -Algorithm SHA256).Hash

function Get-Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $mid = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$mid] }
    return ($sorted[$mid - 1] + $sorted[$mid]) / 2
}

function Get-Percentile([double[]]$Sorted, [double]$Percentile) {
    return $Sorted[[int][Math]::Ceiling($Percentile * $Sorted.Count) - 1]
}

function Get-Window([string]$TracePath) {
    $messages = & $csvExport -m -s '|' $TracePath
    if ($LASTEXITCODE -ne 0) { throw "Tracy message export failed: $TracePath" }

    $begin = @($messages | Where-Object { $_ -match 'BENCH: transform window begin\|\d+$' })
    $end = @($messages | Where-Object { $_ -match 'BENCH: transform window end\|\d+$' })
    if ($begin.Count -ne 1 -or $end.Count -ne 1) {
        throw "Expected both measurement window markers exactly once: $TracePath"
    }
    $first = [long]($begin[0].Split('|')[-1])
    $last = [long]($end[0].Split('|')[-1])
    if ($last -le $first) { throw "Invalid measurement window: $TracePath" }

    $workload = @($messages | Where-Object { $_ -match 'BENCH: transform entities=50000 workers=\d+\|' })
    if ($workload.Count -ne 1 -or $workload[0] -notmatch 'workers=(\d+)\|') {
        throw "Expected a startup marker for 50000 entities: $TracePath"
    }
    return @{ First = $first; Last = $last; Workers = [int]$Matches[1] }
}

function Get-Zone([string]$TracePath, [string]$Name, [long]$First, [long]$Last) {
    $durations = [Collections.Generic.List[double]]::new()
    $threads = [Collections.Generic.HashSet[string]]::new()
    $firstStart = [long]::MaxValue
    $lastEnd = [long]::MinValue
    $count = 0

    # Stream through the range events, which can take up much more space than frames.
    & $csvExport -u -s '|' -f $Name $TracePath | ForEach-Object {
        $fields = $_.Split('|')
        if ($fields.Count -lt 6 -or $fields[0] -cne $Name) { return }
        $start = [long]$fields[3]
        $duration = [long]$fields[4]
        $end = $start + $duration
        if ($start -lt $First -or $end -gt $Last) { return }

        ++$count
        [void]$threads.Add($fields[5])
        $firstStart = [Math]::Min($firstStart, $start)
        $lastEnd = [Math]::Max($lastEnd, $end)
        if ($Name -ne 'ParallelForRange' -and $Name -ne 'ParallelFor') {
            $durations.Add($duration / 1000000.0)
        }
    }
    if ($LASTEXITCODE -ne 0) { throw "Tracy zone export failed for ${Name}: $TracePath" }

    return @{ Count = $count; Durations = $durations; Threads = $threads; First = $firstStart; Last = $lastEnd }
}

function Get-RunMetrics([string]$TracePath, [string]$Mode, [int]$Run) {
    $window = Get-Window $TracePath
    $frame = Get-Zone $TracePath 'Frame' $window.First $window.Last
    if ($frame.Count -eq 0 -or $frame.Threads.Count -ne 1) {
        throw "Expected full frames on one main thread: $TracePath"
    }
    # Match all measured zones to the same complete frames.
    $zones = @{
        transform = 'TransformSystem'; gather = 'TransformGather'; sort = 'TransformSort'
        matrices = 'TransformMatrices'; swap = 'Swap'
    }
    $result = [ordered]@{
        mode = $Mode; run = $Run; entities = 50000; workers = $window.Workers
        window_ms = [Math]::Round(($window.Last - $window.First) / 1000000.0, 3)
        frames = $frame.Count; scene_sha256 = $sceneHash
    }
    foreach ($key in @('frame', 'transform', 'gather', 'sort', 'matrices', 'swap')) {
        $zone = if ($key -eq 'frame') { $frame } else {
            Get-Zone $TracePath $zones[$key] $frame.First $frame.Last
        }
        if ($zone.Count -ne $frame.Count) {
            throw "Expected one $key event per complete frame; found $($zone.Count) for $($frame.Count) frames: $TracePath"
        }
        $result["${key}_median_ms"] = [Math]::Round((Get-Median $zone.Durations.ToArray()), 6)
        if ($key -in @('frame', 'transform')) {
            $sorted = [double[]]($zone.Durations | Sort-Object)
            $result["${key}_p95_ms"] = [Math]::Round((Get-Percentile $sorted 0.95), 6)
            $result["${key}_p99_ms"] = [Math]::Round((Get-Percentile $sorted 0.99), 6)
        }
    }

    $parallel = Get-Zone $TracePath 'ParallelFor' $frame.First $frame.Last
    $ranges = Get-Zone $TracePath 'ParallelForRange' $frame.First $frame.Last
    $result['parallel_calls'] = $parallel.Count
    $result['range_events'] = $ranges.Count
    $result['range_worker_threads'] = @($ranges.Threads | Where-Object { -not $frame.Threads.Contains($_) }).Count

    if ($Mode -eq 'serial' -and ($window.Workers -ne 0 -or $parallel.Count -ne 0 -or $ranges.Count -ne 0)) {
        throw "Serial run unexpectedly scheduled worker jobs: $TracePath"
    }
    if ($Mode -eq 'parallel' -and ($window.Workers -eq 0 -or $result['range_worker_threads'] -eq 0)) {
        throw "Parallel run did not execute transform ranges on worker threads: $TracePath"
    }
    return [pscustomobject]$result
}

$results = [Collections.Generic.List[object]]::new()
$statuses = [Collections.Generic.List[object]]::new()
for ($run = 1; $run -le $Runs; ++$run) {
    foreach ($mode in ($Modes | Select-Object -Unique)) {
        $name = "transform-${mode}_run$run"
        $tracePath = Join-Path $traces "$name.tracy"
        $capture = $null
        $game = $null
        $failure = ''
        Write-Host "Capture $name"

        try {
            $capture = Start-Process -FilePath $captureExe -ArgumentList @('-o', ('"' + $tracePath + '"'), '-a', '127.0.0.1') `
                -PassThru -WindowStyle Hidden `
                -RedirectStandardOutput (Join-Path $logs "$name.capture.txt") `
                -RedirectStandardError (Join-Path $logs "$name.capture.err.txt")
            Start-Sleep -Milliseconds 700
            if ($capture.HasExited) { throw "Capture exited before the game started." }

            $game = Start-Process -FilePath $exe -WorkingDirectory $ExeDir -ArgumentList @("--bench=transform-$mode", '--novsync') `
                -PassThru -WindowStyle Hidden `
                -RedirectStandardOutput (Join-Path $logs "$name.game.txt") `
                -RedirectStandardError (Join-Path $logs "$name.game.err.txt")

            if (-not $game.WaitForExit(60000)) { throw 'Game timed out.' }
            if ($game.ExitCode -ne 0) { throw "Game exit code: $($game.ExitCode)" }
            if (-not $capture.WaitForExit(30000)) { throw 'Capture did not finish after the game exited.' }
            if ($capture.ExitCode -ne 0) { throw "Capture exit code: $($capture.ExitCode)" }
            if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) { throw 'Trace file was not written.' }

            $results.Add((Get-RunMetrics $tracePath $mode $run))
            $results | Export-Csv -LiteralPath (Join-Path $OutDir 'metrics.csv') -NoTypeInformation -Encoding UTF8
        } catch {
            $failure = $_.Exception.Message
            Write-Warning "$name failed: $failure"
        } finally {
            foreach ($process in @($game, $capture)) {
                if ($null -ne $process -and -not $process.HasExited) {
                    Stop-Process -InputObject $process -Force -ErrorAction SilentlyContinue
                    [void]$process.WaitForExit(5000)
                }
            }
            $statuses.Add([pscustomobject]@{ mode = $mode; run = $run; ok = ($failure -eq ''); error = $failure })
            $statuses | Export-Csv -LiteralPath (Join-Path $OutDir 'runs.csv') -NoTypeInformation -Encoding UTF8
        }
    }
}

if ($results.Count -gt 0) {
    $summary = foreach ($group in ($results | Group-Object mode)) {
        $row = [ordered]@{ mode = $group.Name; runs = $group.Count }
        foreach ($property in $group.Group[0].PSObject.Properties) {
            $column = $property.Name
            if ($column -notin @('mode', 'run', 'scene_sha256')) {
                $values = [double[]]($group.Group | ForEach-Object { $_.$column })
                $row[$column] = [Math]::Round((Get-Median $values), 6)
            }
        }
        [pscustomobject]$row
    }
    $summary | Export-Csv -LiteralPath (Join-Path $OutDir 'summary.csv') -NoTypeInformation -Encoding UTF8
    $summary | Format-Table mode, runs, workers, frames, transform_median_ms, frame_median_ms
}

Write-Host "Results: $OutDir"
if (@($statuses | Where-Object { -not $_.ok }).Count) {
    throw "One or more captures failed. See runs.csv in $OutDir."
}

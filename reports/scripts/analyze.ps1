# Подсчёт метрик по трейсам Tracy, записанным measure.ps1.
# Выход: metrics.csv (по каждому прогону) и summary.csv (медиана по прогонам для каждого сценария).
param(
    [string]$InDir    = "H:\MGEngine\reports\baseline",
    [string]$TracyDir = "H:\MGEngine\windows-0.14.1",
    [int]$Workers = 2            # число потоков загрузки в LoadQueue (для загрузки воркеров)
)

$csvexport = Join-Path $TracyDir "tracy-csvexport.exe"
$inv = [Globalization.CultureInfo]::InvariantCulture
# Числа всегда через точку (иначе на русской Windows дробная запятая ломает CSV)
[Threading.Thread]::CurrentThread.CurrentCulture = $inv

# --- Вспомогательные функции ---

# Все вызовы зоны с точным именем: start/dur в наносекундах, thread
function Get-Zones([string]$trace, [string]$name) {
    $lines = & $csvexport -u -f $name $trace 2>$null
    $result = New-Object System.Collections.Generic.List[object]
    foreach ($l in ($lines | Select-Object -Skip 1)) {
        $p = $l.Split(',')
        if ($p.Count -lt 6 -or $p[0] -ne $name) { continue }
        $result.Add([pscustomobject]@{
            Start  = [double]::Parse($p[3], $inv)
            Dur    = [double]::Parse($p[4], $inv)
            Thread = $p[5]
        })
    }
    return $result
}

# Сообщения: текст может содержать запятые, поэтому делим по последней запятой
function Get-Messages([string]$trace) {
    $lines = & $csvexport -m $trace 2>$null
    foreach ($l in ($lines | Select-Object -Skip 1)) {
        $i = $l.LastIndexOf(',')
        if ($i -lt 0) { continue }
        [pscustomobject]@{ Text = $l.Substring(0, $i); Time = [double]::Parse($l.Substring($i + 1), $inv) }
    }
}

# Перцентиль "по ближайшему рангу"
function Get-Pct([double[]]$sorted, [double]$p) {
    if ($sorted.Count -eq 0) { return [double]::NaN }
    $k = [Math]::Ceiling($p * $sorted.Count) - 1
    if ($k -lt 0) { $k = 0 }
    return $sorted[[int]$k]
}

function Sum-InWindow($zones, [double]$ws, [double]$we) {
    $s = 0.0
    foreach ($z in $zones) { if ($z.Start -lt $we -and ($z.Start + $z.Dur) -gt $ws) { $s += $z.Dur } }
    return $s
}

# --- Обработка трейсов ---

$rows = New-Object System.Collections.Generic.List[object]
$traces = Get-ChildItem (Join-Path $InDir "traces") -Filter *.tracy | Sort-Object Name

foreach ($t in $traces) {
    $scenario = $t.BaseName -replace '_run\d+$', ''
    $run = [int]($t.BaseName -replace '^.*_run', '')
    Write-Host "analyze $($t.Name)"

    # @(...) обязательно: если зона одна, PowerShell 5.1 вернёт объект без .Count
    $msgs   = @(Get-Messages $t.FullName)
    $frames = @(Get-Zones $t.FullName "Frame")
    $submit = @(Get-Zones $t.FullName "BenchSubmit")
    $pump   = @(Get-Zones $t.FullName "PumpUploads")
    $lsync  = @(Get-Zones $t.FullName "LoadSync")
    $upTex  = @(Get-Zones $t.FullName "UploadTexture")
    $upMesh = @(Get-Zones $t.FullName "UploadMesh")
    $parseT = @(Get-Zones $t.FullName "ParseTexture")
    $parseM = @(Get-Zones $t.FullName "ParseMesh")
    $shut   = @(Get-Zones $t.FullName "Shutdown")

    $mScenario = $msgs | Where-Object { $_.Text -like "*BENCH: scenario*" } | Select-Object -First 1
    $mDone     = $msgs | Where-Object { $_.Text -like "*BENCH: batch done*" } | Select-Object -First 1
    $mExit     = $msgs | Where-Object { $_.Text -like "*BENCH: exit requested*" } | Select-Object -First 1

    # Окно измерения
    if ($scenario -eq "steady") {
        $ws = $mScenario.Time + 3e9                                   # после прогрева
        $we = ($frames | Measure-Object Start -Maximum).Maximum        # до последнего кадра
    } else {
        $ws = ($submit | Select-Object -First 1).Start
        if ($mDone)     { $we = $mDone.Time }
        elseif ($mExit) { $we = $mExit.Time }
        else            { $we = ($frames | Measure-Object Start -Maximum).Maximum }
    }

    # Кадры, пересекающие окно
    $fw = @($frames | Where-Object { $_.Start -lt $we -and ($_.Start + $_.Dur) -gt $ws } | ForEach-Object { $_.Dur / 1e6 })
    $sorted = [double[]]($fw | Sort-Object)
    $windowMs = ($we - $ws) / 1e6

    $parseSum = (Sum-InWindow $parseT 0 1e18) + (Sum-InWindow $parseM 0 1e18)
    $pumpMax  = if ($pump.Count) { (($pump | Where-Object { $_.Start -lt $we -and ($_.Start + $_.Dur) -gt $ws } | Measure-Object Dur -Maximum).Maximum) / 1e6 } else { 0 }

    $rows.Add([pscustomobject]@{
        scenario           = $scenario
        run                = $run
        window_ms          = [Math]::Round($windowMs, 1)
        frames             = $fw.Count
        frame_median_ms    = [Math]::Round((Get-Pct $sorted 0.50), 3)
        frame_p95_ms       = [Math]::Round((Get-Pct $sorted 0.95), 3)
        frame_p99_ms       = [Math]::Round((Get-Pct $sorted 0.99), 3)
        frame_max_ms       = [Math]::Round((Get-Pct $sorted 1.00), 1)
        frames_over_16_7   = @($fw | Where-Object { $_ -gt 16.7 }).Count
        frames_over_33_3   = @($fw | Where-Object { $_ -gt 33.3 }).Count
        main_loadsync_ms   = [Math]::Round((Sum-InWindow $lsync $ws $we) / 1e6, 1)
        main_pump_ms       = [Math]::Round((Sum-InWindow $pump $ws $we) / 1e6, 1)
        main_pump_max_ms   = [Math]::Round($pumpMax, 1)
        main_upload_ms     = [Math]::Round(((Sum-InWindow $upTex $ws $we) + (Sum-InWindow $upMesh $ws $we)) / 1e6, 1)
        parse_total_ms     = [Math]::Round($parseSum / 1e6, 1)
        worker_util_pct    = if ($scenario -eq "load-async" -and $windowMs -gt 0) { [Math]::Round(100 * ($parseSum / 1e6) / ($windowMs * $Workers), 1) } else { "" }
        shutdown_ms        = if ($shut.Count) { [Math]::Round($shut[0].Dur / 1e6, 1) } else { "" }
    })
}

$metricsCsv = Join-Path $InDir "metrics.csv"
$rows | ConvertTo-Csv -NoTypeInformation | ForEach-Object { $_ -replace '"', '' } | Set-Content $metricsCsv -Encoding UTF8

# --- Медианы по прогонам ---
function Median($vals) {
    $v = [double[]]($vals | Where-Object { "$_" -ne "" } | ForEach-Object { [double]$_ } | Sort-Object)
    if ($v.Count -eq 0) { return "" }
    if ($v.Count % 2) { return $v[($v.Count - 1) / 2] }
    return ($v[$v.Count / 2 - 1] + $v[$v.Count / 2]) / 2
}
$cols = $rows[0].PSObject.Properties.Name | Where-Object { $_ -notin "scenario", "run" }
$summary = foreach ($g in ($rows | Group-Object scenario)) {
    $o = [ordered]@{ scenario = $g.Name; runs = $g.Count }
    foreach ($c in $cols) {
        $m = Median ($g.Group | ForEach-Object { $_.$c })
        $o[$c] = if ($m -is [double]) { [Math]::Round($m, 3) } else { $m }
    }
    [pscustomobject]$o
}
$summaryCsv = Join-Path $InDir "summary.csv"
$summary | ConvertTo-Csv -NoTypeInformation | ForEach-Object { $_ -replace '"', '' } | Set-Content $summaryCsv -Encoding UTF8
$summary | Format-List

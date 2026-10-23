# Прогоны режима замера MGEngine с записью трейсов Tracy.
# Для каждого сценария и прогона: запускается tracy-capture, затем игра с --bench=<сценарий>;
# игра сама грузит пачку и закрывается, capture сам завершается после отключения клиента.
param(
    [string]$ExeDir   = "H:\MGEngine\MGEngine\build\windows-x64-profile",
    [string]$TracyDir = "H:\MGEngine\windows-0.14.1",
    [string]$OutDir   = "H:\MGEngine\reports\baseline",
    [int]$Runs = 5,
    [string[]]$Scenarios = @("steady", "load-sync", "load-async", "load-exit"),
    [switch]$VSync      # по умолчанию прогоны идут с --novsync
)

$traces = Join-Path $OutDir "traces"
$logs   = Join-Path $OutDir "logs"
New-Item -ItemType Directory -Force $traces, $logs | Out-Null
$runsCsv = Join-Path $OutDir "runs.csv"
if (-not (Test-Path $runsCsv)) { "scenario,run,exit_code,wall_s,capture_ok,trace_mb" | Set-Content $runsCsv -Encoding UTF8 }

$exe = Join-Path $ExeDir "MyGameEngine.exe"
$cap = Join-Path $TracyDir "tracy-capture.exe"

foreach ($sc in $Scenarios) {
    for ($r = 1; $r -le $Runs; $r++) {
        $name  = "{0}_run{1}" -f $sc, $r
        $trace = Join-Path $traces "$name.tracy"
        $gameLog = Join-Path $ExeDir "game.log"
        if (Test-Path $gameLog) { Remove-Item $gameLog }

        # 1. Сначала запись - она ждёт подключения клиента
        $capProc = Start-Process $cap -ArgumentList "-o", "`"$trace`"", "-f", "-a", "127.0.0.1" `
                   -PassThru -WindowStyle Hidden `
                   -RedirectStandardOutput (Join-Path $logs "$name.capture.txt") `
                   -RedirectStandardError  (Join-Path $logs "$name.capture.err.txt")
        Start-Sleep -Milliseconds 700

        # 2. Игра
        $args = @("--bench=$sc")
        if (-not $VSync) { $args += "--novsync" }
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $game = Start-Process $exe -ArgumentList $args -WorkingDirectory $ExeDir -PassThru
        $gameOk = $game.WaitForExit(300000)
        $sw.Stop()
        if (-not $gameOk) { Stop-Process -Id $game.Id -Force; $code = "timeout" } else { $code = $game.ExitCode }

        # 3. Запись должна закончиться сама после отключения клиента
        $capOk = $capProc.WaitForExit(120000)
        if (-not $capOk) { Stop-Process -Id $capProc.Id -Force }

        if (Test-Path $gameLog) { Copy-Item $gameLog (Join-Path $logs "$name.game.log") -Force }
        $mb = if (Test-Path $trace) { [math]::Round((Get-Item $trace).Length / 1MB, 1) } else { 0 }
        # InvariantCulture: дробная часть через точку, иначе запятая сломает CSV
        $inv = [Globalization.CultureInfo]::InvariantCulture
        [string]::Format($inv, "{0},{1},{2},{3:F2},{4},{5}", $sc, $r, $code, $sw.Elapsed.TotalSeconds, $capOk, $mb) |
            Add-Content $runsCsv -Encoding UTF8
        Write-Host ("{0,-16} exit={1,-8} wall={2,6:N1}s capture={3} trace={4} MB" -f $name, $code, $sw.Elapsed.TotalSeconds, $capOk, $mb)

        Start-Sleep -Seconds 2   # пауза между прогонами
    }
}

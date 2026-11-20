# Generate the deterministic TransformSystem benchmark scene.
[CmdletBinding()]
param(
    [string]$OutFile = (Join-Path $PSScriptRoot '..\assets\bench\transform_scene.json')
)

$ErrorActionPreference = 'Stop'
$OutFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutFile)
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutFile))

# Each group has one root, two children and two grandchildren.
$scene = [Text.StringBuilder]::new()
[void]$scene.AppendLine('{"version":1,"entities":[')
$rootCount = 10000
for ($i = 0; $i -lt $rootCount; ++$i) {
    $x = $i % 100
    $z = [int][Math]::Floor($i / 100)
    if ($i -gt 0) { [void]$scene.AppendLine(',') }

    [void]$scene.Append(('{{"id":"root-{0}","components":{{"Transform":{{"position":[{1},0,{2}]}}}}}},' -f $i, $x, $z))
    [void]$scene.Append(('{{"id":"left-{0}","parent":"root-{0}","components":{{"Transform":{{"position":[-2,0,0],"rotation":[0,0,1,0]}}}}}},' -f $i))
    [void]$scene.Append(('{{"id":"right-{0}","parent":"root-{0}","components":{{"Transform":{{"position":[2,0,0]}}}}}},' -f $i))
    [void]$scene.Append(('{{"id":"left-tip-{0}","parent":"left-{0}","components":{{"Transform":{{"position":[1,1,0]}}}}}},' -f $i))
    [void]$scene.Append(('{{"id":"right-tip-{0}","parent":"right-{0}","components":{{"Transform":{{"position":[1,1,0]}}}}}}' -f $i))
}
[void]$scene.AppendLine()
[void]$scene.AppendLine(']}')

[IO.File]::WriteAllText($OutFile, $scene.ToString(), [Text.UTF8Encoding]::new($false))
Write-Output "Generated 50000 entities (10000 / 20000 / 20000 by depth): $OutFile"

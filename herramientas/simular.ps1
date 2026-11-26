# Simula partidas de swarm con un bot, sin ventana, y resume cuanto duran por ataque
# inicial (percentiles) y cuando llegan los hitos de nivel. Sirve para ajustar la
# curva de dificultad (ver ideas/swarm.md). Usa la misma logica que el widget:
# compila herramientas\swarm_sim.c, que incluye widgets\swarm\swarm.c.
#
# Uso:  .\herramientas\simular.ps1            (100 partidas por ataque inicial)
#       .\herramientas\simular.ps1 400 7      (400 por ataque, semilla 7)
#       .\herramientas\simular.ps1 100 0 200  (el bot decide cada 200 ms; sin esto, ~175 ms: calibrado con partidas reales)
param([int]$Partidas = 100, [int]$Semilla = 0, [int]$Reaccion = 0)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command zig -ErrorAction SilentlyContinue)) {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path
}
$exe = Join-Path $env:TEMP "fidgets_swarm_sim.exe"
& zig cc -target x86_64-windows-gnu -O2 -std=c11 -fno-math-errno -Wall -Wno-missing-braces `
    -I (Join-Path $root "common") (Join-Path $PSScriptRoot "swarm_sim.c") (Join-Path $root "common\widget.c") `
    -luser32 -lgdi32 -lwtsapi32 -o $exe
if ($LASTEXITCODE -ne 0) { throw "no compilo swarm_sim.c" }
& $exe $Partidas $Semilla $Reaccion

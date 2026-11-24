# Compila cada widget de widgets/<nombre>/ a bin/<nombre>.exe
# Uso:  .\build.ps1            (todos)
#       .\build.ps1 dot        (solo uno)
param([string]$Only = "")

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$bin = Join-Path $root "bin"
New-Item -ItemType Directory -Force $bin | Out-Null

if (-not (Get-Command zig -ErrorAction SilentlyContinue)) {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path
}

$flags = @(
    "-target", "x86_64-windows-gnu",
    "-Os", "-s", "-std=c11",
    "-fno-math-errno",
    "-Wall", "-Wno-missing-braces",
    "-Wl,--subsystem,windows",
    "-I", (Join-Path $root "common")
)
$libs = @("-luser32", "-lgdi32")

$failed = 0
Get-ChildItem (Join-Path $root "widgets") -Directory | Where-Object { -not $Only -or $_.Name -eq $Only } | ForEach-Object {
    $name = $_.Name
    $c = @(Get-ChildItem $_.FullName -Filter *.c | ForEach-Object FullName)
    if ($c.Count -eq 0) { return }   # carpeta aun sin codigo
    $src = $c + (Join-Path $root "common\widget.c")
    $out = Join-Path $bin "$name.exe"
    & zig cc @flags @src @libs -o $out
    if ($LASTEXITCODE -ne 0) { Write-Host "FALLO  $name" -ForegroundColor Red; $failed++ }
    else { "{0,-12} {1,6:N1} KB" -f $name, ((Get-Item $out).Length / 1KB) }
}
Remove-Item (Join-Path $bin "*.pdb") -ErrorAction SilentlyContinue
exit $failed

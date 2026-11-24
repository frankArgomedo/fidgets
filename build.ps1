# Compila cada widget de widgets/<nombre>/ a bin/<nombre>.exe, con su icono
# (widgets/<nombre>/icon.ico) y VERSIONINFO (version en VERSION).
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
$libs = @("-luser32", "-lgdi32", "-lwtsapi32")
$MAX_KB = 128   # tope del .exe (ver "Animacion y consumo" en CLAUDE.md)

$version = (Get-Content (Join-Path $root "VERSION") -TotalCount 1).Trim()
if ($version -notmatch '^(\d+)\.(\d+)\.(\d+)$') { throw "VERSION invalida: $version" }
$vnum = "$($Matches[1]),$($Matches[2]),$($Matches[3]),0"
$tmp = Join-Path $env:TEMP "fidgets_build"
New-Item -ItemType Directory -Force $tmp | Out-Null

# Recursos de un widget: icono y VERSIONINFO (FileDescription es lo que muestra el
# Administrador de tareas). Devuelve la ruta del .res.
function New-Resources($name, $dir) {
    $ico = (Join-Path $dir "icon.ico") -replace '\\', '\\'   # en .rc la barra se escribe doble
    $rc = Join-Path $tmp "$name.rc"; $res = Join-Path $tmp "$name.res"
    $lines = @()
    if (Test-Path (Join-Path $dir "icon.ico")) { $lines += "1 ICON `"$ico`"" }
    $lines += @(
        "1 VERSIONINFO", "FILEVERSION $vnum", "PRODUCTVERSION $vnum", "FILEOS 0x40004", "FILETYPE 1",
        "BEGIN", " BLOCK `"StringFileInfo`"", " BEGIN", "  BLOCK `"040904B0`"", "  BEGIN",
        "   VALUE `"CompanyName`", `"frankArgomedo`"",
        "   VALUE `"FileDescription`", `"$name`"",
        "   VALUE `"FileVersion`", `"$version`"",
        "   VALUE `"InternalName`", `"$name`"",
        "   VALUE `"LegalCopyright`", `"Copyright (c) 2026 frankArgomedo. MIT License.`"",
        "   VALUE `"OriginalFilename`", `"$name.exe`"",
        "   VALUE `"ProductName`", `"fidgets`"",
        "   VALUE `"ProductVersion`", `"$version`"",
        "  END", " END", " BLOCK `"VarFileInfo`"", " BEGIN", "  VALUE `"Translation`", 0x409, 1200", " END", "END")
    $lines | Set-Content $rc -Encoding ascii
    & zig rc /fo $res $rc
    if ($LASTEXITCODE -ne 0) { throw "zig rc fallo para $name" }
    return $res
}

$failed = 0
Get-ChildItem (Join-Path $root "widgets") -Directory | Where-Object { -not $Only -or $_.Name -eq $Only } | ForEach-Object {
    $name = $_.Name
    $c = @(Get-ChildItem $_.FullName -Filter *.c | ForEach-Object FullName)
    if ($c.Count -eq 0) { return }   # carpeta aun sin codigo
    $src = $c + (Join-Path $root "common\widget.c") + (New-Resources $name $_.FullName)
    $out = Join-Path $bin "$name.exe"
    & zig cc @flags @src @libs -o $out
    if ($LASTEXITCODE -ne 0) { Write-Host "FALLO  $name" -ForegroundColor Red; $failed++ }
    else {
        $kb = (Get-Item $out).Length / 1KB
        if ($kb -gt $MAX_KB) { Write-Host ("{0,-12} {1,6:N1} KB  PASA EL TOPE DE {2} KB" -f $name, $kb, $MAX_KB) -ForegroundColor Yellow }
        else { "{0,-12} {1,6:N1} KB" -f $name, $kb }
    }
}
Remove-Item (Join-Path $bin "*.pdb") -ErrorAction SilentlyContinue
exit $failed

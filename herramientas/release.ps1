# Arma los archivos de una version para GitHub Releases, en dist\<version>\:
#   fidgets-<widget>-<version>.zip  un .zip por widget (su .exe, LICENSE y los README)
#   fidgets-<version>.zip           todos los widgets juntos
#   SHA256SUMS.txt                  SHA-256 de cada .exe y cada .zip
# La version sale de VERSION. Compila desde cero y exige un arbol limpio (sin cambios
# sin commit), para que lo publicado sea exactamente lo que hay en el repositorio.
#
# Uso:  .\herramientas\release.ps1          (version de VERSION)
#       .\herramientas\release.ps1 -Prueba  (permite cambios sin commit; no publicar)
param([switch]$Prueba)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$version = (Get-Content (Join-Path $root "VERSION") -TotalCount 1).Trim()
if (-not (Select-String -Path CHANGELOG.md -Pattern "^## \[$([regex]::Escape($version))\]" -Quiet)) {
  throw "CHANGELOG.md no tiene la seccion [$version]"
}
# cambios en archivos del repositorio, o archivos nuevos que entrarian en la compilacion
$dirty = @(git status --porcelain --untracked-files=no) + @(git status --porcelain -- common widgets | Where-Object { $_ -like "??*" })
$dirty = $dirty | Where-Object { $_ }
if ($dirty -and -not $Prueba) { throw "Hay cambios sin commit; haz commit o usa -Prueba:`n$($dirty -join "`n")" }
$tag = git tag --points-at HEAD
if ($tag -notcontains "v$version") { Write-Host "Aviso: HEAD no tiene el tag v$version" -ForegroundColor Yellow }

# compilar desde cero
Remove-Item (Join-Path $root "bin\*.exe") -ErrorAction SilentlyContinue
& (Join-Path $root "build.ps1")
if ($LASTEXITCODE -ne 0) { throw "build.ps1 fallo" }

$out = Join-Path $root "dist\$version"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
$stage = Join-Path $env:TEMP "fidgets_release"

$docs = @("LICENSE", "README.md", "README.es.md")
$widgets = Get-ChildItem (Join-Path $root "widgets") -Directory | Where-Object { Get-ChildItem $_.FullName -Filter *.c } | ForEach-Object Name
$zips = @()

function New-Zip($name, $exes) {
  if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
  New-Item -ItemType Directory $stage | Out-Null
  foreach ($e in $exes) { Copy-Item (Join-Path $root "bin\$e.exe") $stage }
  foreach ($d in $docs) { Copy-Item (Join-Path $root $d) $stage }
  # typing lleva dentro las listas de palabras (CC BY-SA 4.0): va su aviso de licencia
  if ($exes -contains "typing") { Copy-Item (Join-Path $root "widgets\typing\words\LEEME.md") (Join-Path $stage "WORDS-LICENSE.md") }
  $zip = Join-Path $out $name
  Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
  return $zip
}

foreach ($w in $widgets) { $zips += New-Zip "fidgets-$w-$version.zip" @($w) }
$zips += New-Zip "fidgets-$version.zip" $widgets
Remove-Item $stage -Recurse -Force

# sumas: .exe y .zip, en el formato de sha256sum ("hash  archivo")
$sums = @()
foreach ($w in $widgets) { $sums += "{0}  {1}.exe" -f (Get-FileHash (Join-Path $root "bin\$w.exe") -Algorithm SHA256).Hash.ToLower(), $w }
foreach ($z in $zips) { $sums += "{0}  {1}" -f (Get-FileHash $z -Algorithm SHA256).Hash.ToLower(), (Split-Path $z -Leaf) }
$sums | Set-Content (Join-Path $out "SHA256SUMS.txt") -Encoding ascii

""
"Version $version en $out"
Get-ChildItem $out | ForEach-Object { "  {0,-32} {1,7:N1} KB" -f $_.Name, ($_.Length / 1KB) }
""
$sums

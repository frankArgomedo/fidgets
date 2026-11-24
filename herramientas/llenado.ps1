# Comprueba, sin abrir ventanas, que el contenido de cada widget respete el margen
# minimo (MARGIN = 6 px) en cada estado, y que cada GRUPO de estados llene la celda:
# juntando todos sus estados, en al menos un eje ambos lados quedan a <= LLENO px.
# (P.ej. el cubo se dimensiona por el angulo de camara mas ancho, no por cada uno.)
#
# Uso:  .\herramientas\llenado.ps1            (todos los casos)
#       .\herramientas\llenado.ps1 cubo       (solo los de un widget)
# Usa los .exe de bin\ (compilar antes). Sale con codigo 1 si algun caso falla.
param([string]$Only = "", [switch]$Detalle)

$MARGIN = 6      # igual que MARGIN en common/widget.h
$LLENO = 10      # un eje "lleno": sus dos margenes <= 10 px
$UMBRAL = 24     # alfa a partir del cual un pixel cuenta como contenido visible

$root = Split-Path $PSScriptRoot -Parent
$tmp = Join-Path $env:TEMP "bored_llenado"
New-Item -ItemType Directory -Force $tmp | Out-Null

# Casos: widget, argumentos, ini, teclas (scan hex), texto (on_char), ms simulados, nombre,
#        estado transitorio (puede usar el margen), grupo para el llenado
$casos = @()
foreach ($t in -6, -3, 0, 3, 6) { foreach ($p in -6, 0, 6) {
  $casos += , @("cubo", "", "[cubo]`r`ncam_t=$t`r`ncam_p=$p`r`ngiro_ms=0", "", "", 0, "cubo camara t=$t p=$p", $false, "cubo (todas las camaras)")
} }
# giros a mitad de animacion (100 ms por giro; 50 ms = 45 grados), camara de frente y girada
foreach ($cam in @("cam_t=0`r`ncam_p=6", "cam_t=6`r`ncam_p=6")) {
  foreach ($k in @(@("24", "U"), @("17", "R"), @("23", "F"), @("27", "y"))) {
    $casos += , @("cubo", "", "[cubo]`r`n$cam`r`ngiro_ms=100", $k[0], "", 50, "cubo giro $($k[1]) a 45 ($($cam -replace "`r`n", ' '))", $true, "")
  }
}
$casos += , @("cubo", "--teclas", "", "", "", 0, "mapa reposo", $false, "mapa")
$casos += , @("cubo", "--teclas", "", "17", "", 60, "mapa tecla iluminada", $false, "mapa")
$casos += , @("teclear", "", "", "", "", 0, "teclear reposo", $false, "teclear texto")
$casos += , @("teclear", "", "[teclear]`r`npuntuacion=1`r`nnumeros=1`r`nidioma=1`r`nlista=1", "", "", 0, "teclear puntuacion", $false, "teclear texto")
$casos += , @("teclear", "", "", "", "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", 0, "teclear letras de mas", $false, "teclear texto")
$casos += , @("teclear", "", "[teclear]`r`nmodo=1`r`npalabras=0", "", "q w e r t y u i o p ", 0, "teclear resultado", $false, "teclear resultado")
$casos += , @("teclear", "", "[teclear]`r`ndificultad=2", "", "##", 0, "teclear fallo", $false, "teclear fallo")
$casos += , @("punto", "", "", "", "", 0, "punto reposo", $false, "")   # area de juego: los puntos ocupan toda la celda al jugar (no se puede simular el clic)

$fallos = 0
$grupos = [ordered]@{}
foreach ($c in $casos) {
  if ($Only -and $c[0] -ne $Only) { continue }
  $exe = Join-Path $root "bin\$($c[0]).exe"
  "$($c[2])" | Set-Content "$tmp\p.ini" -Encoding Unicode
  $env:BORED_INI = "$tmp\p.ini"; $env:BORED_SNAPSHOT = "$tmp\f.bsnp"; $env:BORED_TICK_MS = "$($c[5])"
  if ($c[3]) { $env:BORED_KEYS = $c[3] } else { Remove-Item Env:BORED_KEYS -ErrorAction SilentlyContinue }
  if ($c[4]) { $env:BORED_TEXT = $c[4] } else { Remove-Item Env:BORED_TEXT -ErrorAction SilentlyContinue }
  if ($c[1]) { Start-Process $exe -ArgumentList $c[1] -Wait } else { Start-Process $exe -Wait }
  $d = [IO.File]::ReadAllBytes("$tmp\f.bsnp"); $w = [BitConverter]::ToInt32($d, 4); $h = [BitConverter]::ToInt32($d, 8)
  $x0 = $w; $y0 = $h; $x1 = -1; $y1 = -1
  for ($y = 0; $y -lt $h; $y++) { $row = 12 + $y * $w * 4; for ($x = 0; $x -lt $w; $x++) {
    if ($d[$row + $x * 4 + 3] -gt $UMBRAL) { if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }; if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y } }
  } }
  $l = $x0; $t = $y0; $r = $w - 1 - $x1; $b = $h - 1 - $y1
  $min = [Math]::Min([Math]::Min($l, $r), [Math]::Min($t, $b))
  if ($c[8]) {
    if (-not $grupos.Contains($c[8])) { $grupos[$c[8]] = @($w, $h, $w, $h, -1, -1) }
    $g = $grupos[$c[8]]
    $g[2] = [Math]::Min($g[2], $x0); $g[3] = [Math]::Min($g[3], $y0); $g[4] = [Math]::Max($g[4], $x1); $g[5] = [Math]::Max($g[5], $y1)
  }
  # los estados transitorios (giros de 100 ms) pueden usar el margen, pero no salirse de la ventana
  $ok = if ($c[7]) { $min -ge 0 -and $x0 -gt 0 -and $y0 -gt 0 -and $x1 -lt $w - 1 -and $y1 -lt $h - 1 } else { $min -ge $MARGIN }
  $nota = if ($ok) { "ok" } else { "FALLA: margen $min < $MARGIN" }
  if (-not $ok) { $fallos++ }
  if ($Detalle -or -not $ok) { "{0,-40} {1}x{2}  izq {3,3} arr {4,3} der {5,3} aba {6,3}  {7}" -f $c[6], $w, $h, $l, $t, $r, $b, $nota }
}
"--- llenado por grupo (todos sus estados juntos)"
foreach ($k in $grupos.Keys) {
  $g = $grupos[$k]
  $l = $g[2]; $t = $g[3]; $r = $g[0] - 1 - $g[4]; $b = $g[1] - 1 - $g[5]
  $ejeLleno = ($l -le $LLENO -and $r -le $LLENO) -or ($t -le $LLENO -and $b -le $LLENO)
  # sobrante por eje (sin contar los margenes): 40 px o mas = cabria en una celda menor
  $sx = $l + $r - 2 * $MARGIN; $sy = $t + $b - 2 * $MARGIN
  $escalon = $sx -ge 40 -or $sy -ge 40
  $nota = if (-not $ejeLleno) { "SOBRA ESPACIO (ningun eje lleno)" } elseif ($escalon) { "SOBRA UN ESCALON (sobrante $sx x $sy)" } else { "lleno (sobrante $sx x $sy)" }
  "{0,-40} izq {1,3} arr {2,3} der {3,3} aba {4,3}  {5}" -f $k, $l, $t, $r, $b, $nota
  if (-not $ejeLleno -or $escalon) { $fallos++ }
}
Remove-Item Env:BORED_SNAPSHOT, Env:BORED_TICK_MS, Env:BORED_INI, Env:BORED_KEYS, Env:BORED_TEXT -ErrorAction SilentlyContinue
"{0} fallos" -f $fallos
exit [int]($fallos -gt 0)

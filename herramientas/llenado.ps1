# Comprueba, sin abrir ventanas, que el contenido de cada widget respete el margen
# minimo (MARGIN = 6 px) en cada estado, y que cada GRUPO de estados llene la celda:
# juntando todos sus estados, en al menos un eje ambos lados quedan a <= LLENO px.
# (P.ej. el cubo se dimensiona por el angulo de camara mas ancho, no por cada uno.)
#
# Uso:  .\herramientas\llenado.ps1            (todos los casos)
#       .\herramientas\llenado.ps1 cube       (solo los de un widget)
# Usa los .exe de bin\ (compilar antes). Sale con codigo 1 si algun caso falla.
param([string]$Only = "", [switch]$Detalle)

$MARGIN = 6      # igual que MARGIN en common/widget.h
$LLENO = 10      # un eje "lleno": sus dos margenes <= 10 px
$UMBRAL = 24     # alfa a partir del cual un pixel cuenta como contenido visible

$root = Split-Path $PSScriptRoot -Parent
$tmp = Join-Path $env:TEMP "fidgets_llenado"
New-Item -ItemType Directory -Force $tmp | Out-Null

# Casos: widget, argumentos, ini, eventos (FIDGETS_EVENTS, ver widget.h), nombre,
#        estado transitorio (puede usar el margen), grupo para el llenado.
# Cada caso se fotografia en cada idioma (lang:es y lang:en antes de sus eventos).
$casos = @()
foreach ($t in -6, -3, 0, 3, 6) { foreach ($p in -6, 0, 6) {
  $casos += , @("cube", "", "[cube]`r`ncam_t=$t`r`ncam_p=$p`r`nturn_ms=0", "", "cube camara t=$t p=$p", $false, "cube (todas las camaras)")
} }
# giros a mitad de animacion (100 ms por giro; 50 ms = 45 grados), camara de frente y girada
foreach ($cam in @("cam_t=0`r`ncam_p=6", "cam_t=6`r`ncam_p=6")) {
  foreach ($k in @(@("24", "U"), @("17", "R"), @("23", "F"), @("27", "y"))) {
    $casos += , @("cube", "", "[cube]`r`n$cam`r`nturn_ms=100", "key:$($k[0]) tick:50", "cube giro $($k[1]) a 45 ($($cam -replace "`r`n", ' '))", $true, "")
  }
}
$casos += , @("cube", "--keys", "", "", "mapa reposo", $false, "mapa")
$casos += , @("cube", "--keys", "", "key:17 tick:60", "mapa tecla iluminada", $false, "mapa")
$casos += , @("typing", "", "", "", "typing reposo", $false, "typing texto")
$casos += , @("typing", "", "[typing]`r`npunctuation=1`r`nnumbers=1`r`nlang=1`r`nlist=1000", "", "typing puntuacion", $false, "typing texto")
$casos += , @("typing", "", "", "text:qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", "typing letras de mas", $false, "typing texto")
$casos += , @("typing", "", "[typing]`r`nmode=1`r`nwords=10", "text:q\sw\se\sr\st\sy\su\si\so\sp\s", "typing resultado", $false, "typing resultado")
$casos += , @("typing", "", "[typing]`r`ndifficulty=2", "text:##", "typing fallo", $false, "typing fallo")
$casos += , @("dot", "", "", "", "dot reposo", $false, "")
# jugando: los puntos aparecen al azar en toda la celda (no cuenta para el llenado)
$casos += , @("dot", "", "", "click:80,68 tick:300", "dot jugando", $false, "")

$fallos = 0
$grupos = [ordered]@{}
$env:FIDGETS_SEED = "1"
foreach ($c0 in $casos) { foreach ($lang in "es", "en") {
  $c = @($c0[0], $c0[1], $c0[2], "lang:$lang $($c0[3])", "$($c0[4]) [$lang]", $c0[5], $c0[6])
  if ($Only -and $c[0] -ne $Only) { continue }
  $exe = Join-Path $root "bin\$($c[0]).exe"
  "$($c[2])" | Set-Content "$tmp\p.ini" -Encoding Unicode
  $env:FIDGETS_INI = "$tmp\p.ini"; $env:FIDGETS_SNAPSHOT = "$tmp\f.bsnp"; $env:FIDGETS_EVENTS = $c[3]
  if ($c[1]) { Start-Process $exe -ArgumentList $c[1] -Wait } else { Start-Process $exe -Wait }
  $d = [IO.File]::ReadAllBytes("$tmp\f.bsnp"); $w = [BitConverter]::ToInt32($d, 4); $h = [BitConverter]::ToInt32($d, 8)
  $x0 = $w; $y0 = $h; $x1 = -1; $y1 = -1
  for ($y = 0; $y -lt $h; $y++) { $row = 12 + $y * $w * 4; for ($x = 0; $x -lt $w; $x++) {
    if ($d[$row + $x * 4 + 3] -gt $UMBRAL) { if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }; if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y } }
  } }
  $l = $x0; $t = $y0; $r = $w - 1 - $x1; $b = $h - 1 - $y1
  $min = [Math]::Min([Math]::Min($l, $r), [Math]::Min($t, $b))
  if ($c[6]) {
    if (-not $grupos.Contains($c[6])) { $grupos[$c[6]] = @($w, $h, $w, $h, -1, -1) }
    $g = $grupos[$c[6]]
    $g[2] = [Math]::Min($g[2], $x0); $g[3] = [Math]::Min($g[3], $y0); $g[4] = [Math]::Max($g[4], $x1); $g[5] = [Math]::Max($g[5], $y1)
  }
  # los estados transitorios (giros de 100 ms) pueden usar el margen, pero no salirse de la ventana
  $ok = if ($c[5]) { $min -ge 0 -and $x0 -gt 0 -and $y0 -gt 0 -and $x1 -lt $w - 1 -and $y1 -lt $h - 1 } else { $min -ge $MARGIN }
  $nota = if ($ok) { "ok" } else { "FALLA: margen $min < $MARGIN" }
  if (-not $ok) { $fallos++ }
  if ($Detalle -or -not $ok) { "{0,-40} {1}x{2}  izq {3,3} arr {4,3} der {5,3} aba {6,3}  {7}" -f $c[4], $w, $h, $l, $t, $r, $b, $nota }
} }
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
Remove-Item Env:FIDGETS_SNAPSHOT, Env:FIDGETS_EVENTS, Env:FIDGETS_INI, Env:FIDGETS_SEED -ErrorAction SilentlyContinue
"{0} fallos" -f $fallos
exit [int]($fallos -gt 0)

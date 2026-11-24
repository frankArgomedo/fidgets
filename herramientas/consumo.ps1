# Mide el consumo de cada widget sin taparle nada al usuario: abre el widget en modo
# medicion (FIDGETS_MEASURE), que pone la ventana fuera de todos los monitores, sin
# atajos globales y con otro titulo. Mide:
#   - memoria privada en reposo (MB)
#   - CPU en reposo durante 3 s (ms de CPU; lo esperado es ~0)
#   - CPU animando durante 3 s (% de un nucleo) y tiempo medio de dibujo por frame
# Para animar le manda mensajes a su ventana (teclas o clics), como haria el usuario.
#
# Uso:  .\herramientas\consumo.ps1            (todos)
#       .\herramientas\consumo.ps1 cube       (solo uno)
#       .\herramientas\consumo.ps1 -Bin ruta  (usar otra carpeta de .exe)
# Compilar antes. Sale con codigo 1 si algun widget pasa un tope (ver $TOPES).
param([string]$Only = "", [string]$Bin = "")

# Topes (ver "Animacion y consumo" en CLAUDE.md). Medidas del 2026-11-24: memoria
# 1,5-1,8 MB (cubo 5,3 con las tablas del solver), 0 ms de CPU en reposo, 0,3-1,5 ms
# por frame. Los topes dejan margen; un widget que los pase lo explica en su ficha.
$TOPES = @{
  exe_kb = 128          # tamano del .exe
  mem_mb = 2.5          # memoria privada (la mayor entre reposo y animando)
  idle_ms = 5           # ms de CPU en 3 s de reposo
  frame_ms = 3          # tiempo medio de dibujo por frame
}
$EXCEPCIONES = @{ "cube" = @{ mem_mb = 7 } }   # solver de dos fases: ~3,7 MB de tablas

$root = Split-Path $PSScriptRoot -Parent
if (-not $Bin) { $Bin = Join-Path $root "bin" }
$tmp = Join-Path $env:TEMP "fidgets_consumo"
New-Item -ItemType Directory -Force $tmp | Out-Null

Add-Type -Namespace Fidgets -Name Win -MemberDefinition @'
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
[DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
[DllImport("user32.dll")] public static extern uint GetDpiForSystem();
[DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
'@
# misma escala que la base (que es "system aware"): para que los clics caigan donde deben
[void][Fidgets.Win]::SetProcessDpiAwarenessContext([IntPtr](-2))
$S = [Math]::Max(1, [int][Math]::Floor(([Fidgets.Win]::GetDpiForSystem() + 48) / 96))
$WM_KEYDOWN = 0x100; $WM_CHAR = 0x102; $WM_LBUTTONDOWN = 0x201; $WM_LBUTTONUP = 0x202; $WM_CLOSE = 0x10

function Send-Key($h, $vk, $scan) { [void][Fidgets.Win]::PostMessageW($h, $WM_KEYDOWN, [IntPtr]$vk, [IntPtr](($scan -shl 16) -bor 1)) }
function Send-Char($h, $ch) { [void][Fidgets.Win]::PostMessageW($h, $WM_CHAR, [IntPtr][int][char]$ch, [IntPtr]1) }
function Send-Click($h, $x, $y) {
  $lp = [IntPtr]((($y * $S) -shl 16) -bor ($x * $S))
  [void][Fidgets.Win]::PostMessageW($h, $WM_LBUTTONDOWN, [IntPtr]1, $lp)
  [void][Fidgets.Win]::PostMessageW($h, $WM_LBUTTONUP, [IntPtr]0, $lp)
}

# Casos: nombre, exe, argumentos, como animar (se llama cada 100 ms con el numero de paso)
$casos = @(
  # espacio = mezclar (arma las tablas del solver), luego J = giro U cada 200 ms
  @("cube", "cube", "", { param($h, $i) if ($i -eq 0) { Send-Key $h 0x20 0x39 } elseif ($i % 2 -eq 0) { Send-Key $h 0x4A 0x24 } }),
  @("keys", "cube", "--keys", { param($h, $i) if ($i % 3 -eq 0) { Send-Key $h 0x49 0x17 } }),     # I: ilumina
  @("dot", "dot", "", { param($h, $i) if ($i -eq 0) { Send-Click $h 80 68 } }),                     # jugar
  @("typing", "typing", "", { param($h, $i) Send-Char $h 'a' })                                    # escribir
)

$fallos = 0
"{0,-8} {1,7} {2,8} {3,9} {4,10} {5,7} {6,9}" -f "widget", "exe KB", "mem MB", "CPU rep.", "CPU anim.", "frames", "ms/frame"
foreach ($c in $casos) {
  if ($Only -and $c[0] -ne $Only) { continue }
  $exe = Join-Path $Bin "$($c[1]).exe"
  "" | Set-Content "$tmp\m.ini" -Encoding Unicode
  Remove-Item "$tmp\m.txt" -ErrorAction SilentlyContinue
  # esperar a que se haya cerrado la ventana del caso anterior
  for ($i = 0; $i -lt 30 -and [Fidgets.Win]::FindWindowW("fidgets_widget", "fidgets-measure") -ne [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100 }
  $env:FIDGETS_INI = "$tmp\m.ini"; $env:FIDGETS_MEASURE = "$tmp\m.txt"
  $p = if ($c[2]) { Start-Process $exe -ArgumentList $c[2] -PassThru } else { Start-Process $exe -PassThru }
  $h = [IntPtr]::Zero
  for ($i = 0; $i -lt 50 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; $h = [Fidgets.Win]::FindWindowW("fidgets_widget", "fidgets-measure") }
  if ($h -eq [IntPtr]::Zero) { "{0,-8} no se encontro la ventana" -f $c[0]; $fallos++; Stop-Process $p -ErrorAction SilentlyContinue; continue }

  Start-Sleep -Milliseconds 1500                     # que termine de arrancar
  $p.Refresh(); $mem = $p.PrivateMemorySize64 / 1MB
  $t0 = $p.TotalProcessorTime; Start-Sleep -Seconds 3; $p.Refresh()
  $idle = ($p.TotalProcessorTime - $t0).TotalMilliseconds

  $t0 = $p.TotalProcessorTime; $sw = [Diagnostics.Stopwatch]::StartNew()
  for ($i = 0; $i -lt 30; $i++) { & $c[3] $h $i; Start-Sleep -Milliseconds 100 }
  $p.Refresh(); $anim = ($p.TotalProcessorTime - $t0).TotalMilliseconds / $sw.Elapsed.TotalMilliseconds * 100
  $memAnim = $p.PrivateMemorySize64 / 1MB
  if ($memAnim -gt $mem) { $mem = $memAnim }   # p.ej. el cubo arma las tablas del solver al mezclar

  [void][Fidgets.Win]::PostMessageW($h, $WM_CLOSE, [IntPtr]0, [IntPtr]0)
  if (-not $p.WaitForExit(3000)) { Stop-Process $p -ErrorAction SilentlyContinue }
  $st = @{}; if (Test-Path "$tmp\m.txt") { Get-Content "$tmp\m.txt" | ForEach-Object { $k, $v = $_ -split "="; $st[$k] = [int]$v } }
  $fr = [int]$st["frames"]; $us = [int]$st["render_us"]
  $msf = if ($fr) { $us / $fr / 1000 } else { 0 }
  $kb = (Get-Item $exe).Length / 1KB

  $tope = $TOPES.Clone(); if ($EXCEPCIONES.ContainsKey($c[0])) { foreach ($k in $EXCEPCIONES[$c[0]].Keys) { $tope[$k] = $EXCEPCIONES[$c[0]][$k] } }
  $mal = @()
  if ($kb -gt $tope.exe_kb) { $mal += "exe" }
  if ($mem -gt $tope.mem_mb) { $mal += "memoria" }
  if ($idle -gt $tope.idle_ms) { $mal += "CPU en reposo" }
  if ($msf -gt $tope.frame_ms) { $mal += "ms/frame" }
  $nota = if ($mal) { "PASA EL TOPE: " + ($mal -join ", ") } else { "ok" }
  if ($mal) { $fallos++ }
  "{0,-8} {1,7:N1} {2,8:N2} {3,7:N0}ms {4,9:N1}% {5,7} {6,9:N3}  {7}" -f $c[0], $kb, $mem, $idle, $anim, $fr, $msf, $nota
}
Remove-Item Env:FIDGETS_INI, Env:FIDGETS_MEASURE -ErrorAction SilentlyContinue
"{0} fallos" -f $fallos
exit [int]($fallos -gt 0)

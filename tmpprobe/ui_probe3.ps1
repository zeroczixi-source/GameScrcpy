
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -Namespace Q -Name W -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
'@
$exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe"
$dir = "C:\Users\zero\Downloads\GameScrcpy-main\tmpprobe"
$out = "$dir\shot_report.txt"
Set-Content -Encoding UTF8 -Path $out -Value "shot report"
function W($s) { Add-Content -Encoding UTF8 -Path $out -Value $s }
function Get-Win($id) {
  $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $id)
  $els = [System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $cond)
  foreach ($e in $els) { if ($e.Current.NativeWindowHandle -ne 0 -and $e.Current.Name -notlike "*帮助*") { return $e } }
  return $null
}
function Get-WinByTitle($id, $t) {
  $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $id)
  $els = [System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $cond)
  foreach ($e in $els) { if ($e.Current.NativeWindowHandle -ne 0 -and $e.Current.Name -like "*$t*") { return $e } }
  return $null
}
function Shot($name) {
  $w = Get-Win $procId
  $r = $w.Current.BoundingRectangle
  $bmp = New-Object System.Drawing.Bitmap([int]$r.Width, [int]$r.Height)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen([int]$r.X, [int]$r.Y, 0, 0, $bmp.Size)
  $bmp.Save("$dir\$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  W ("SHOT {0} rect={1},{2} {3}x{4}" -f $name, [int]$r.X, [int]$r.Y, [int]$r.Width, [int]$r.Height)
}
function WinSize($ww, $hh) {
  $win = Get-Win $procId
  [Q.W]::SetWindowPos([IntPtr]$win.Current.NativeWindowHandle, [IntPtr]::Zero, 0, 0, $ww, $hh, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 900
}
function SbState($tag) {
  $w = Get-Win $procId
  $all = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  $wr = $w.Current.BoundingRectangle
  $n = 0; $descr = ""
  for ($i=0; $i -lt $all.Count; $i++) {
    $e = $all.Item($i); $c = $e.Current
    if ($c.ControlType.ProgrammaticName -eq "ControlType.ScrollBar") { $n++; $b = $c.BoundingRectangle; $descr += (" sb{0}={1},{2} {3}x{4}" -f $n, [int]$b.X, [int]$b.Y, [int]$b.Width, [int]$b.Height) }
  }
  W ("STATE {0} win={1},{2} {3}x{4} scrollbars={5}{6}" -f $tag, [int]$wr.X, [int]$wr.Y, [int]$wr.Width, [int]$wr.Height, $n, $descr)
}

cmd /c start "" "$exe"
Start-Sleep -Seconds 9
$p = Get-Process GameScrcpy -ErrorAction SilentlyContinue
if ($p -eq $null) { W "PROCESS NOT FOUND"; exit }
$procId = $p.Id
WinSize 700 500
Shot "s1_home_min"
SbState "home_min"
WinSize 1150 700
Shot "s2_home_big"

$w = Get-Win $procId
$btns = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)))
$nav = @()
for ($i=0; $i -lt $btns.Count; $i++) { $e = $btns.Item($i); $b = $e.Current.BoundingRectangle; if ([int]$b.X -lt 220 -and $b.Width -gt 0) { $nav += $e } }
$nav = $nav | Sort-Object -Property @{Expression={ $_.Current.BoundingRectangle.Y }}
W ("navbtn " + $nav.Count)
$nav[3].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 1200
WinSize 700 500
Shot "s3_settings_min"
SbState "settings_min"
WinSize 900 700
Shot "s4_settings_860"
SbState "settings_900"
WinSize 1150 700
Shot "s5_settings_big"
SbState "settings_big"

$win = Get-Win $procId
[Q.W]::ShowWindow([IntPtr]$win.Current.NativeWindowHandle, 3) | Out-Null
Start-Sleep -Milliseconds 1500
Shot "s6_settings_max"
SbState "settings_max"
[Q.W]::ShowWindow([IntPtr]$win.Current.NativeWindowHandle, 1) | Out-Null
Start-Sleep -Milliseconds 1500
Shot "s7_settings_restore"
SbState "settings_restore"

$w = Get-Win $procId
$btns = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)))
$nav = @()
for ($i=0; $i -lt $btns.Count; $i++) { $e = $btns.Item($i); $b = $e.Current.BoundingRectangle; if ([int]$b.X -lt 220 -and $b.Width -gt 0) { $nav += $e } }
$nav = $nav | Sort-Object -Property @{Expression={ $_.Current.BoundingRectangle.Y }}
W ("navbtn2 " + $nav.Count)
try { $nav[5].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch { W "help click failed" }
Start-Sleep -Milliseconds 1500
$hwin = Get-WinByTitle $procId "帮助"
if ($hwin -ne $null) {
  $hr = $hwin.Current.BoundingRectangle
  W ("HELP win {0},{1} {2}x{3}" -f [int]$hr.X, [int]$hr.Y, [int]$hr.Width, [int]$hr.Height)
  [Q.W]::SetWindowPos([IntPtr]$hwin.Current.NativeWindowHandle, [IntPtr]::Zero, 60, 40, 640, 420, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 1200
  $hwin = Get-WinByTitle $procId "帮助"
  $hr = $hwin.Current.BoundingRectangle
  $bmp = New-Object System.Drawing.Bitmap([int]$hr.Width, [int]$hr.Height)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen([int]$hr.X, [int]$hr.Y, 0, 0, $bmp.Size)
  $bmp.Save("$dir\s8_help_small.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  W ("SHOT s8_help_small rect={0},{1} {2}x{3}" -f [int]$hr.X, [int]$hr.Y, [int]$hr.Width, [int]$hr.Height)
  [Q.W]::SetWindowPos([IntPtr]$hwin.Current.NativeWindowHandle, [IntPtr]::Zero, 60, 40, 1150, 700, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 1200
  $hwin = Get-WinByTitle $procId "帮助"
  $hr = $hwin.Current.BoundingRectangle
  $bmp = New-Object System.Drawing.Bitmap([int]$hr.Width, [int]$hr.Height)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen([int]$hr.X, [int]$hr.Y, 0, 0, $bmp.Size)
  $bmp.Save("$dir\s9_help_big.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  W ("SHOT s9_help_big rect={0},{1} {2}x{3}" -f [int]$hr.X, [int]$hr.Y, [int]$hr.Width, [int]$hr.Height)
  $all = $hwin.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  for ($i=0; $i -lt $all.Count; $i++) { $c = $all.Item($i).Current; $b = $c.BoundingRectangle; W ("H[{0}] {1} | {2} | {3},{4} {5}x{6}" -f $i, $c.ControlType.ProgrammaticName, $c.Name, [int]$b.X, [int]$b.Y, [int]$b.Width, [int]$b.Height) }
} else { W "NO HELP WINDOW" }
Stop-Process -Id $procId -Force
W "DONE"

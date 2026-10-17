param(
  [string]$Exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe",
  [string]$OutDir = "C:\Users\zero\Downloads\GameScrcpy-main\promo\out\ui",
  [int]$W = 1600,
  [int]$H = 1000,
  [int]$WaitMs = 4000
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$sig = @'
using System;
using System.Runtime.InteropServices;
public class Native {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
Add-Type -TypeDefinition $sig

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$exePath = (Resolve-Path $Exe).Path
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exePath
$psi.WorkingDirectory = (Split-Path $exePath)
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
"PID=$($proc.Id)"

$hwnd = [IntPtr]::Zero
$deadline = (Get-Date).AddSeconds(45)
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  $proc.Refresh()
  if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $proc.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { "NO_WINDOW"; $proc.Kill(); exit 2 }
"HWND=$hwnd Title=$($proc.MainWindowTitle)"

[Native]::ShowWindow($hwnd, 9) | Out-Null
[Native]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, $W, $H, 0x0040) | Out-Null
Start-Sleep -Milliseconds 400
[Native]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds $WaitMs

$r = New-Object Native+RECT
[Native]::GetWindowRect($hwnd, [ref]$r) | Out-Null
"RECT=$($r.Left),$($r.Top) $($r.Right - $r.Left)x$($r.Bottom - $r.Top)"

# 1) PrintWindow (renders off-screen, full size)
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Magenta)
$hdc = $g.GetHdc()
$ok = [Native]::PrintWindow($hwnd, $hdc, 2)
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save((Join-Path $OutDir "printwindow.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"PRINTWINDOW=$ok"

# 2) screen copy of the window area
$ww = [Math]::Min($r.Right - $r.Left, 1280)
$hh = [Math]::Min($r.Bottom - $r.Top, 720)
$bmp2 = New-Object System.Drawing.Bitmap($ww, $hh)
$g2 = [System.Drawing.Graphics]::FromImage($bmp2)
$g2.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($ww, $hh)))
$g2.Dispose()
$bmp2.Save((Join-Path $OutDir "screen.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp2.Dispose()
"SCREEN=${ww}x${hh}"

# 3) UI Automation tree dump
try {
  $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
  $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  $lines = New-Object System.Collections.Generic.List[string]
  foreach ($e in $all) {
    $c = $e.Current
    $bb = $c.BoundingRectangle
    $name = ($c.Name -replace "`r?`n", " ")
    $lines.Add(("{0}`t{1}`t[{2},{3} {4}x{5}]" -f $c.ControlType.ProgrammaticName, $name, [int]$bb.X, [int]$bb.Y, [int]$bb.Width, [int]$bb.Height))
  }
  $lines | Set-Content -Encoding UTF8 (Join-Path $OutDir "uia.txt")
  "UIA_ELEMENTS=$($all.Count)"
} catch {
  "UIA_FAIL=$($_.Exception.Message)"
}

$proc.Kill()
"KILLED"

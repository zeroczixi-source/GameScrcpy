param(
  [string]$Exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe",
  [string]$OutDir = "C:\Users\zero\Downloads\GameScrcpy-main\promo\out\ui"
)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
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
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
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
"HWND=$hwnd alive=$(-not $proc.HasExited)"
[Native]::ShowWindow($hwnd, 9) | Out-Null
[Native]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1300, 740, 0x0040) | Out-Null
Start-Sleep -Milliseconds 3000

$r = New-Object Native+RECT
[Native]::GetWindowRect($hwnd, [ref]$r) | Out-Null
"RECT=$($r.Left),$($r.Top) $($r.Right-$r.Left)x$($r.Bottom-$r.Top)"

$bmp = New-Object System.Drawing.Bitmap(1400, 800)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Magenta)
$hdc = $g.GetHdc()
$ok = [Native]::PrintWindow($hwnd, $hdc, 2)
$g.ReleaseHdc($hdc); $g.Dispose()
$bmp.Save((Join-Path $OutDir 'dbg_00home.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"SHOT home ok=$ok alive=$(-not $proc.HasExited)"

# list invokable buttons before click
$root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
$all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
"ELEMENTS=$($all.Count)"
foreach ($e in $all) {
  $c = $e.Current
  if ($c.ControlType.ProgrammaticName -eq 'ControlType.Button') {
    $pats = @()
    foreach ($pt in @('Invoke','Toggle','SelectionItem')) {
      $p = $null
      if ($e.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$p)) { }
    }
    "BTN name='$($c.Name)' rect=$([int]$c.BoundingRectangle.X),$([int]$c.BoundingRectangle.Y) $([int]$c.BoundingRectangle.Width)x$([int]$c.BoundingRectangle.Height)"
  }
}

$target = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, '设置')))
"FOUND settings: $($target -ne $null) alive=$(-not $proc.HasExited)"
if ($target -ne $null) {
  $inv = $target.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
  "GOT invoke pattern"
  try { $inv.Invoke(); "INVOKED" } catch { "INVOKE_ERR=$($_.Exception.Message)" }
  Start-Sleep -Milliseconds 2500
  $proc.Refresh()
  "after click: alive=$(-not $proc.HasExited) isWindow=$([Native]::IsWindow($hwnd)) hwndNow=$($proc.MainWindowHandle) title=$($proc.MainWindowTitle)"
  $h2 = $proc.MainWindowHandle
  if ($h2 -ne [IntPtr]::Zero) { $hwnd = $h2 }
  $bmp2 = New-Object System.Drawing.Bitmap(1400, 800)
  $g2 = [System.Drawing.Graphics]::FromImage($bmp2)
  $g2.Clear([System.Drawing.Color]::Magenta)
  $hdc2 = $g2.GetHdc()
  $ok2 = [Native]::PrintWindow($hwnd, $hdc2, 2)
  $g2.ReleaseHdc($hdc2); $g2.Dispose()
  $bmp2.Save((Join-Path $OutDir 'dbg_01settings.png'), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp2.Dispose()
  "SHOT settings ok=$ok2"
}
if (-not $proc.HasExited) { $proc.Kill() }
'DONE'

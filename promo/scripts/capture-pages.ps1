param(
  [string]$Exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe",
  [string]$OutDir = "C:\Users\zero\Downloads\GameScrcpy-main\promo\out\ui"
)
# ASCII-only on purpose: powershell.exe 5.1 reads BOM-less files as ANSI.
# Sidebar buttons are located by index in UI Automation order (1=home 2=settings 3=terminal 4=help).

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
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wParam, IntPtr lParam);
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
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 300
  $proc.Refresh()
  $h = $proc.MainWindowHandle
  if ($h -ne [IntPtr]::Zero -and [Native]::IsWindowVisible($h)) {
    $r = New-Object Native+RECT
    [Native]::GetWindowRect($h, [ref]$r) | Out-Null
    if (($r.Right - $r.Left) -gt 200) { $hwnd = $h; break }
  }
}
if ($hwnd -eq [IntPtr]::Zero) { 'NO_WINDOW'; if (-not $proc.HasExited) { $proc.Kill() }; exit 2 }

[Native]::ShowWindow($hwnd, 9) | Out-Null
[Native]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1300, 740, 0x0040) | Out-Null
[Native]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 3500

function Get-WinRect([IntPtr]$h) {
  $r = New-Object Native+RECT
  [Native]::GetWindowRect($h, [ref]$r) | Out-Null
  return $r
}

function Save-Shot([IntPtr]$h, [string]$path) {
  $r = Get-WinRect $h
  $w = $r.Right - $r.Left
  $hh = $r.Bottom - $r.Top
  if ($w -le 0 -or $hh -le 0) { return $null }
  $bmp = New-Object System.Drawing.Bitmap($w, $hh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::Magenta)
  $hdc = $g.GetHdc()
  [Native]::PrintWindow($h, $hdc, 2) | Out-Null
  $g.ReleaseHdc($hdc)
  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  return @{ w = $w; h = $hh }
}

function Get-Buttons([IntPtr]$h) {
  $root = [System.Windows.Automation.AutomationElement]::FromHandle($h)
  $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  $out = @()
  foreach ($e in $all) {
    $c = $e.Current
    if ($c.ControlType.ProgrammaticName -eq 'ControlType.Button') { $out += , $e }
  }
  return $out
}

function Dump-Tree([IntPtr]$h) {
  $root = [System.Windows.Automation.AutomationElement]::FromHandle($h)
  $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  $items = @()
  foreach ($e in $all) {
    $c = $e.Current
    $bb = $c.BoundingRectangle
    $items += [pscustomobject]@{
      type = ($c.ControlType.ProgrammaticName -replace 'ControlType\.', '')
      name = ($c.Name -replace "`r?`n", ' ')
      x    = [int]$bb.X
      y    = [int]$bb.Y
      w    = [int]$bb.Width
      h    = [int]$bb.Height
    }
  }
  return $items
}

function Send-Wheel([IntPtr]$h, [int]$notches) {
  $r = Get-WinRect $h
  $cx = [int](($r.Right - $r.Left) / 2) + $r.Left
  $cy = [int](($r.Bottom - $r.Top) / 2) + $r.Top
  $lparam = [IntPtr](($cy -shl 16) -bor ($cx -band 0xFFFF))
  for ($i = 0; $i -lt $notches; $i++) {
    $wparam = [IntPtr]((-120) -shl 16)
    [Native]::SendMessage($h, 0x020A, $wparam, $lparam) | Out-Null
    Start-Sleep -Milliseconds 160
  }
}

$steps = @(
  @{ id = 'home'; index = 1 },
  @{ id = 'settings'; index = 2 },
  @{ id = 'terminal'; index = 3 }
)

$manifest = @()
foreach ($step in $steps) {
  $proc.Refresh()
  if ($proc.HasExited) { "PROC_EXITED before $($step.id)"; break }
  $buttons = Get-Buttons $hwnd
  if ($buttons.Count -le $step.index) { "MISS $($step.id) count=$($buttons.Count)"; continue }
  $el = $buttons[$step.index]
  $invoked = $false
  try {
    $el.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    $invoked = $true
  } catch {
    "CLICK_FAIL $($step.id): $($_.Exception.Message)"
  }
  Start-Sleep -Milliseconds 2000
  $file = Join-Path $OutDir ("page_{0}.png" -f $step.id)
  $size = Save-Shot $hwnd $file
  if ($null -eq $size) { "SAVE_FAIL $($step.id)"; continue }
  $tree = @()
  try { $tree = Dump-Tree $hwnd } catch { "TREE_FAIL $($step.id)" }
  $r = Get-WinRect $hwnd
  $manifest += [pscustomobject]@{
    page     = $step.id
    file     = ("page_{0}.png" -f $step.id)
    invoked  = $invoked
    windowX  = $r.Left
    windowY  = $r.Top
    windowW  = $r.Right - $r.Left
    windowH  = $r.Bottom - $r.Top
    elements = $tree
  }
  "SHOT $($step.id) $($size.w)x$($size.h) elements=$($tree.Count)"

  # settings page is taller than the window: capture a scrolled state as a second shot
  if ($step.id -eq 'settings') {
    Send-Wheel $hwnd 8
    Start-Sleep -Milliseconds 1200
    $file2 = Join-Path $OutDir 'page_settings_scrolled.png'
    $size2 = Save-Shot $hwnd $file2
    if ($null -ne $size2) {
      $tree2 = @()
      try { $tree2 = Dump-Tree $hwnd } catch { }
      $r2 = Get-WinRect $hwnd
      $manifest += [pscustomobject]@{
        page     = 'settings_scrolled'
        file     = 'page_settings_scrolled.png'
        invoked  = $true
        windowX  = $r2.Left
        windowY  = $r2.Top
        windowW  = $r2.Right - $r2.Left
        windowH  = $r2.Bottom - $r2.Top
        elements = $tree2
      }
      "SHOT settings_scrolled $($size2.w)x$($size2.h) elements=$($tree2.Count)"
    }
    Send-Wheel $hwnd -12
    Start-Sleep -Milliseconds 800
  }
}

$jsonPath = Join-Path $OutDir 'ui_manifest.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $jsonPath
"META $jsonPath"
if (-not $proc.HasExited) { $proc.Kill() }
'DONE'

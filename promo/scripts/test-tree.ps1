param(
  [string]$Exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe"
)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$sig = @'
using System;
using System.Runtime.InteropServices;
public class Native {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
Add-Type -TypeDefinition $sig

$exePath = (Resolve-Path $Exe).Path
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exePath
$psi.WorkingDirectory = (Split-Path $exePath)
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)

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
"HWND=$hwnd (pid $($proc.Id))"
[Native]::ShowWindow($hwnd, 9) | Out-Null
[Native]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1300, 740, 0x0040) | Out-Null
Start-Sleep -Milliseconds 5000

foreach ($tag in @('t5s', 't8s')) {
  Start-Sleep -Milliseconds 3000
  try {
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
    $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    "$tag elements=$($all.Count) name='$($root.Current.Name)' class='$($root.Current.ClassName)'"
    $n = 0
    foreach ($e in $all) {
      $c = $e.Current
      if ($c.ControlType.ProgrammaticName -eq 'ControlType.Button') {
        "  btn[$n] '$($c.Name)' @ $([int]$c.BoundingRectangle.X),$([int]$c.BoundingRectangle.Y)"
        $n++
        if ($n -ge 12) { break }
      }
    }
  } catch {
    "$tag ERR $($_.Exception.Message)"
  }
}
$proc.Kill()
'DONE'


Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -Namespace P -Name N -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
public struct R { public int L; public int T; public int Rr; public int B; }
'@

$exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe"
$out = "$env:TEMP\ui_report.txt"
Set-Content -Encoding UTF8 -Path $out -Value "report start"

function W($s) { Add-Content -Encoding UTF8 -Path $out -Value $s }

function Get-Win($id) {
  $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $id)
  $root = [System.Windows.Automation.AutomationElement]::RootElement
  $els = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $cond)
  foreach ($e in $els) { if ($e.Current.NativeWindowHandle -ne 0) { return $e } }
  return $null
}

function Dump([string]$tag) {
  $w = Get-Win $procId
  if ($w -eq $null) { W "=== $tag === NO WINDOW"; return }
  $wr = $w.Current.BoundingRectangle
  W ("=== {0} === window {1},{2} {3}x{4} title='{5}'" -f $tag, [int]$wr.X, [int]$wr.Y, [int]$wr.Width, [int]$wr.Height, $w.Current.Name)
  $all = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  W ("COUNT " + $all.Count)
  for ($i=0; $i -lt $all.Count; $i++) {
    $e = $all.Item($i); $c = $e.Current; $b = $c.BoundingRectangle
    W ("[{0}] {1} | {2} | {3},{4} {5}x{6} | off={7}" -f $i, $c.ControlType.ProgrammaticName, $c.Name, [int]$b.X, [int]$b.Y, [int]$b.Width, [int]$b.Height, $c.IsOffscreen)
  }
}

cmd /c start "" "$exe"
Start-Sleep -Seconds 9
$p = Get-Process GameScrcpy -ErrorAction SilentlyContinue
if ($p -eq $null) { W "PROCESS NOT FOUND"; exit }
$procId = $p.Id
W ("pid=" + $procId)

$w = Get-Win $procId
$h = [IntPtr]$w.Current.NativeWindowHandle
[P.N]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, 700, 500, 0x0004) | Out-Null
Start-Sleep -Milliseconds 900

$w = Get-Win $procId
$wr = $w.Current.BoundingRectangle
$lim = [int]$wr.X + 220
$cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
$btns = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cond)
$nav = @()
for ($i=0; $i -lt $btns.Count; $i++) { $e = $btns.Item($i); $b = $e.Current.BoundingRectangle; if ([int]$b.X -lt $lim -and $b.Width -gt 0) { $nav += $e } }
$nav = $nav | Sort-Object -Property @{Expression={ $_.Current.BoundingRectangle.Y }}
W ("NAVBUTTONS " + $nav.Count + " lim=" + $lim)
for ($i=0; $i -lt $nav.Count; $i++) { $b = $nav[$i].Current.BoundingRectangle; W ("NAV[$i] '{0}' {1},{2} {3}x{4}" -f $nav[$i].Current.Name, [int]$b.X, [int]$b.Y, [int]$b.Width, [int]$b.Height) }

function ClickNav($idx) {
  try { $nav[$idx].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch { W ("CLICK ERR " + $_.Exception.Message) }
  Start-Sleep -Milliseconds 900
}
function Resize($ww, $hh) {
  $win = Get-Win $procId
  $hhh = [IntPtr]$win.Current.NativeWindowHandle
  [P.N]::SetWindowPos($hhh, [IntPtr]::Zero, 0, 0, $ww, $hh, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 900
}

for ($k = 1; $k -lt $nav.Count; $k++) {
  ClickNav $k
  Resize 700 500
  Dump ("nav$k-small-700x500")
  Resize 1150 700
  Dump ("nav$k-big-1150x700")
}

Start-Sleep -Milliseconds 300
$root = [System.Windows.Automation.AutomationElement]::RootElement
$cond2 = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $procId)
$tops = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $cond2)
W ("TOPLEVEL COUNT " + $tops.Count)
for ($i=0; $i -lt $tops.Count; $i++) { $e=$tops.Item($i); $b=$e.Current.BoundingRectangle; W ("TOP[{0}] '{1}' {2},{3} {4}x{5} cls={6}" -f $i, $e.Current.Name, [int]$b.X, [int]$b.Y, [int]$b.Width, [int]$b.Height, $e.Current.ClassName) }

Stop-Process -Id $procId -Force
W "DONE"

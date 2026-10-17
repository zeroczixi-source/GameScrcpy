
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -Namespace Q -Name W -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
'@
$exe = "C:\Users\zero\Downloads\GameScrcpy-main\output\x64\Release\GameScrcpy.exe"
$out = "$env:TEMP\nav_report.txt"
Set-Content -Encoding UTF8 -Path $out -Value "nav report"
function W($s) { Add-Content -Encoding UTF8 -Path $out -Value $s }
function Get-Win($id) {
  $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $id)
  $els = [System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $cond)
  foreach ($e in $els) { if ($e.Current.NativeWindowHandle -ne 0 -and $e.Current.Name -notlike "*帮助*") { return $e } }
  return $null
}
function State($tag) {
  $w = Get-Win $procId
  $wr = $w.Current.BoundingRectangle
  $all = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
  $s = "STATE $tag win=$([int]$wr.Width)x$([int]$wr.Height)"
  $sb = 0
  for ($i=0; $i -lt $all.Count; $i++) {
    $c = $all.Item($i).Current
    $b = $c.BoundingRectangle
    if ($c.ControlType.ProgrammaticName -eq "ControlType.Group" -and [int]$b.X -eq [int]$wr.X) { $s += (" NAV=$([int]$b.Width)x$([int]$b.Height)") }
    if ($c.ControlType.ProgrammaticName -eq "ControlType.Pane") { $s += (" PAGE=$([int]$b.Width)x$([int]$b.Height)@$([int]$b.X)") }
    if ($c.ControlType.ProgrammaticName -eq "ControlType.ScrollBar") { $sb++; $s += (" SB$sb=$([int]$b.Width)x$([int]$b.Height)@$([int]$b.X),$([int]$b.Y)") }
    if ($c.ControlType.ProgrammaticName -eq "ControlType.Button" -and $c.Name -like "*设置*" -and [int]$b.Width -lt 300) { $s += (" NAVBTN=$([int]$b.Width)") }
  }
  W $s
}
function WinSize($ww, $hh) {
  $win = Get-Win $procId
  [Q.W]::SetWindowPos([IntPtr]$win.Current.NativeWindowHandle, [IntPtr]::Zero, 0, 0, $ww, $hh, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 900
}
function GetNav {
  $w = Get-Win $procId
  $btns = $w.FindAll([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)))
  $nav = @()
  for ($i=0; $i -lt $btns.Count; $i++) { $e = $btns.Item($i); $b = $e.Current.BoundingRectangle; if ([int]$b.X -lt 220 -and $b.Width -gt 0 -and [int]$b.Width -lt 300) { $nav += $e } }
  return ($nav | Sort-Object -Property @{Expression={ $_.Current.BoundingRectangle.Y }})
}
cmd /c start "" "$exe"
Start-Sleep -Seconds 9
$p = Get-Process GameScrcpy -ErrorAction SilentlyContinue
if ($p -eq $null) { W "PROCESS NOT FOUND"; exit }
$procId = $p.Id
WinSize 700 500
State "init_home"
$nav = GetNav
W ("navcount " + $nav.Count)
$nav[3].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 1200
State "settings_before_toggle"
$nav = GetNav
$nav[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 1500
State "after_collapse"
$nav = GetNav
$nav[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 1500
State "after_expand"
WinSize 1150 700
State "after_expand_big"
WinSize 700 500
State "after_expand_small_again"
Stop-Process -Id $procId -Force
W "DONE"

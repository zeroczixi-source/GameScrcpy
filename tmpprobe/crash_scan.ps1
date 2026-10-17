$ErrorActionPreference = 'Continue'
$root = 'C:\Users\zero\Downloads\GameScrcpy-main'
$logDir = Join-Path $root 'output\x64\Release\logs'

Write-Output "=== log files ==="
Get-ChildItem $logDir -File | Sort-Object LastWriteTime | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize | Out-String -Width 200

$latest = Get-ChildItem $logDir -Filter '*.log' | Sort-Object LastWriteTime | Select-Object -Last 1
Write-Output ("LATEST=" + $latest.FullName)
Write-Output ("LINES=" + (Get-Content $latest.FullName).Count)

Write-Output "=== session starts / exits ==="
Select-String -Path $latest.FullName -Pattern 'GameScrcpy started at|\[Main\]\[exit\]|Application exiting' | ForEach-Object { "$($_.LineNumber): $($_.Line)" }

Write-Output "=== tail 50 ==="
Get-Content $latest.FullName -Tail 50

Write-Output "=== pattern counts (whole file) ==="
foreach ($p in 'WARN','ERROR','MouseDiag','recoil','Fatal','CRASH','heap corruption','not found') {
  $c = (Select-String -Path $latest.FullName -Pattern $p -SimpleMatch).Count
  "$p = $c"
}

Write-Output "=== dump files ==="
Get-ChildItem $root -Recurse -File -Include *.dmp,*.mdmp -ErrorAction SilentlyContinue | Select-Object FullName, Length, LastWriteTime | Format-Table -AutoSize | Out-String -Width 200

Write-Output "=== exe state ==="
Get-Item (Join-Path $root 'output\x64\Release\GameScrcpy.exe') | Select-Object Length, LastWriteTime | Format-List

Write-Output "=== Application event log, last 12h (GameScrcpy / AppError / WER) ==="
$since = (Get-Date).AddHours(-12)
Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=$since} -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'GameScrcpy' -or $_.ProviderName -match 'Application Error|Windows Error Reporting' } |
  Select-Object -First 20 TimeCreated, Id, ProviderName, LevelDisplayName, Message | Format-List | Out-String -Width 300
Write-Output "=== end ==="

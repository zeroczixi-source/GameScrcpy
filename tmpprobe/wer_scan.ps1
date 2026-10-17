$ErrorActionPreference = 'Continue'

Write-Output "=== WER report queues ==="
foreach ($d in @('C:\ProgramData\Microsoft\Windows\WER\ReportQueue','C:\ProgramData\Microsoft\Windows\WER\ReportArchive','C:\ProgramData\Microsoft\Windows\WER\Temp')) {
  if (Test-Path $d) {
    Get-ChildItem $d -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'GameScrcpy' } | Select-Object FullName, LastWriteTime | Format-Table -AutoSize | Out-String -Width 200
  } else { Write-Output ("missing: " + $d) }
}

Write-Output "=== LocalAppData CrashDumps ==="
$cd = Join-Path $env:LOCALAPPDATA 'CrashDumps'
if (Test-Path $cd) {
  Get-ChildItem $cd -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize | Out-String -Width 200
} else { Write-Output ("missing: " + $cd) }

Write-Output "=== any GameScrcpy dmp anywhere in temp/userprofile (last 24h) ==="
$since = (Get-Date).AddHours(-24)
Get-ChildItem $env:TEMP, $env:LOCALAPPDATA, 'C:\Users\zero\Downloads\GameScrcpy-main' -Recurse -File -Include *.dmp,*.mdmp,*.wer -ErrorAction SilentlyContinue |
  Where-Object { $_.LastWriteTime -gt $since } |
  Select-Object FullName, Length, LastWriteTime | Format-Table -AutoSize | Out-String -Width 200

Write-Output "=== existing LocalDumps config ==="
foreach ($hive in @('HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps','HKCU:\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps')) {
  if (Test-Path $hive) { Get-ChildItem $hive -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.Name; (Get-ItemProperty $_.PSPath) | Format-List | Out-String } } else { Write-Output ("missing: " + $hive) }
}

Write-Output "=== try enabling per-user LocalDumps for GameScrcpy.exe ==="
$dumpFolder = 'C:\Users\zero\Downloads\GameScrcpy-main\output\crashdumps'
New-Item -ItemType Directory -Force -Path $dumpFolder | Out-Null
$key = 'HKCU:\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps\GameScrcpy.exe'
try {
  New-Item -Path $key -Force -ErrorAction Stop | Out-Null
  New-ItemProperty -Path $key -Name 'DumpFolder' -Value $dumpFolder -PropertyType ExpandString -Force -ErrorAction Stop | Out-Null
  New-ItemProperty -Path $key -Name 'DumpType' -Value 2 -PropertyType DWord -Force -ErrorAction Stop | Out-Null
  New-ItemProperty -Path $key -Name 'DumpCount' -Value 10 -PropertyType DWord -Force -ErrorAction Stop | Out-Null
  Write-Output "HKCU_LOCALDUMP_SET=OK"
  (Get-ItemProperty $key) | Select-Object DumpFolder, DumpType, DumpCount | Format-List | Out-String
} catch {
  Write-Output ("HKCU_LOCALDUMP_FAILED: " + $_.Exception.Message)
}

Write-Output "=== end ==="

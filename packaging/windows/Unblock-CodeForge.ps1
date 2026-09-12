# ==================================================================
#  Unblock-CodeForge.ps1
#  Removes the Mark-of-the-Web (Zone.Identifier) that Windows adds to
#  files downloaded from the internet. Without it, some Windows builds
#  run extra SmartScreen / Defender checks on every launch.
#
#  Run INSIDE the extracted CodeForge folder:
#    powershell -ExecutionPolicy Bypass -File .\Unblock-CodeForge.ps1
# ==================================================================

$ErrorActionPreference = "SilentlyContinue"

Write-Host ""
Write-Host "CodeForge - unblock downloaded files" -ForegroundColor Cyan
Write-Host "-------------------------------------"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Get-ChildItem -Path $root -Recurse -File | Unblock-File

Write-Host "Done. All files in this folder are unblocked." -ForegroundColor Green
Write-Host "You can now run CodeForge.exe normally."
Write-Host ""

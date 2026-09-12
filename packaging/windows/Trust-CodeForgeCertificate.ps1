# ==================================================================
#  Trust-CodeForgeCertificate.ps1
#  Installs the CodeForge code-signing certificate into the current
#  user's Trusted Root + Trusted People stores. After doing this once,
#  Windows shows "CodeForge Project" as a verified publisher for all
#  CodeForge releases signed with this certificate (instead of
#  "Unknown publisher") and stops flagging the binaries.
#
#  Usage:  powershell -ExecutionPolicy Bypass -File .\Trust-CodeForgeCertificate.ps1 -CertFile .\CodeForge.cer
# ==================================================================
param(
    [Parameter(Mandatory = $true)]
    [string]$CertFile
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $CertFile)) {
    Write-Host "Certificate file not found: $CertFile" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "CodeForge - code signing certificate trust" -ForegroundColor Cyan
Write-Host "-------------------------------------------"
Write-Host "This will import '$CertFile' into your CURRENT USER certificate stores:"
Write-Host "  - Trusted Root Certification Authorities"
Write-Host "  - Trusted People"
Write-Host "No administrator rights are required. You can remove it any time via"
Write-Host "certmgr.msc (look for 'CodeForge')."
Write-Host ""

$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2
$cert.Import((Resolve-Path $CertFile))
Write-Host ("Subject : " + $cert.Subject)
Write-Host ("Thumbpr.: " + $cert.Thumbprint)
Write-Host ("Valid   : " + $cert.NotBefore + " -> " + $cert.NotAfter)
Write-Host ""

Import-Certificate -FilePath (Resolve-Path $CertFile) -CertStoreLocation Cert:\CurrentUser\Root | Out-Null
Import-Certificate -FilePath (Resolve-Path $CertFile) -CertStoreLocation Cert:\CurrentUser\TrustedPeople | Out-Null

Write-Host "Certificate imported successfully." -ForegroundColor Green
Write-Host "Signed CodeForge builds are now recognized as published by 'CodeForge Project'."
Write-Host ""

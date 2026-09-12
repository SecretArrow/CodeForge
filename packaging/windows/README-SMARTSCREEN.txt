CodeForge v1.2.0 — Windows Portable
===================================

First launch / Peluncuran pertama
---------------------------------
Windows may show "Windows protected your PC" (SmartScreen) because this is a
new release. This is normal for any new software — it is not a virus warning.

  EN: Click "More info" -> "Run anyway". (One time only.)
  ID: Klik "More info" -> "Run anyway". (Sekali saja.)

Optional — stop all future warnings / Opsional — hentikan semua peringatan:
  1. powershell -ExecutionPolicy Bypass -File .\Unblock-CodeForge.ps1
  2. powershell -ExecutionPolicy Bypass -File .\Trust-CodeForgeCertificate.ps1 -CertFile .\CodeForge.cer

Verify integrity / Verifikasi keaslian:
  Get-FileHash .\CodeForge.exe -Algorithm SHA256
  (compare with SHA256SUMS.txt in the release page)

Full guide: SMARTSCREEN.md  |  Full guide online: docs/SMARTSCREEN.md

Portable mode / Mode portable:
  Create a folder named "portable" next to CodeForge.exe and all settings,
  logs and recovery data stay inside it — nothing in %APPDATA%.

Offline-first. No telemetry. Ever.

# CodeForge & Windows SmartScreen / Defender — Complete Guide

Panduan lengkap (English + Bahasa Indonesia) kenapa Windows menampilkan peringatan
terhadap aplikasi yang belum lama dirilis, dan langkah aman untuk menjalankannya.

---

## Why does SmartScreen warn about CodeForge? / Kenapa SmartScreen memperingatkan?

Windows SmartScreen uses **file reputation**: every downloaded program's hash and
its publisher signature are checked against Microsoft's cloud reputation service.
Reputation is *earned over time* based on downloads and usage volume. Brand-new
releases from small publishers — even 100% clean, correctly signed ones — have no
reputation yet, so Windows shows:

> *"Windows protected your PC — Microsoft Defender SmartScreen prevented an
> unrecognized app from starting."*

**This is expected for any new software and does not mean the file is malware.**
CodeForge is open source: every line is auditable in this repository, and every
release ships with a SHA256SUMS.txt so you can verify integrity yourself.

SmartScreen menggunakan **reputasi file**: hash program + tanda tangan penerbit
dicek ke layanan reputasi cloud Microsoft. Reputasi *dibangun seiring waktu*.
Rilis baru dari penerbit kecil — bahkan yang 100% bersih dan ditandatangani —
belum punya reputasi, sehingga Windows menampilkan peringatan. Ini wajar dan
**tidak berarti file tersebut malware**. CodeForge open source: seluruh kode bisa
diaudit, dan setiap rilis menyertakan SHA256SUMS.txt untuk verifikasi.

---

## Option A — Run anyway (fastest) / Jalankan langsung (paling cepat)

**EN:** When the blue "Windows protected your PC" dialog appears:
1. Click **More info**.
2. Click **Run anyway**. (Only needed once per release.)

**ID:** Saat dialog biru "Windows protected your PC" muncul:
1. Klik **More info** (Info selengkapnya).
2. Klik **Run anyway** (Jalankan saja). Hanya perlu sekali per rilis.

## Option B — Trust our signing certificate / Percayai sertifikat kami

**EN:** Starting with v1.2.0, CodeForge Windows binaries are **code-signed**
(self-signed certificate by "CodeForge Project"). If you import our certificate
into your certificate stores, Windows shows the verified publisher name instead
of "Unknown publisher", and Defender/SmartScreen heuristics treat the file much
more favorably:

```powershell
powershell -ExecutionPolicy Bypass -File .\Trust-CodeForgeCertificate.ps1 -CertFile .\CodeForge.cer
```

The `CodeForge.cer` file is published with every release and embedded in the
portable zip. This only affects *your current user account* — no admin needed.

**ID:** Mulai v1.2.0, binary Windows CodeForge **ditandatangani secara digital**
(sertifikat self-signed atas nama "CodeForge Project"). Impor sertifikat kami
agar Windows menampilkan nama penerbit terverifikasi:

```powershell
powershell -ExecutionPolicy Bypass -File .\Trust-CodeForgeCertificate.ps1 -CertFile .\CodeForge.cer
```

File `CodeForge.cer` disertakan di setiap rilis dan di dalam zip portable.

## Option C — Portable zip users / Pengguna zip portable

**ID:** Setelah mengekstrak zip, jalankan sekali:

```powershell
powershell -ExecutionPolicy Bypass -File .\Unblock-CodeForge.ps1
```

**EN:** This removes the "downloaded from internet" mark from all extracted
files, so Windows stops showing per-file warnings.

---

## Verify your download / Verifikasi unduhan Anda

Every release publishes `SHA256SUMS.txt`. Verify on Windows:

```powershell
Get-FileHash .\CodeForgeSetup-1.2.0-x64.exe -Algorithm SHA256
```

**ID:** Bandingkan hasilnya dengan `SHA256SUMS.txt` di halaman rilis. Cocok =
file utuh dan bukan hasil manipulasi.

## If Defender flags a false positive / Jika Defender salah deteksi

**EN:** If Windows Defender quarantines CodeForge despite the steps above, please
report it to Microsoft so the signature gets whitelisted (usually within 24–72 h):
https://www.microsoft.com/en-us/wdsi/filesubmission

**ID:** Jika Windows Defender mengkarantina CodeForge, laporkan sebagai false
positive ke Microsoft melalui tautan di atas — biasanya diproses dalam 24–72 jam.

## Why not an EV certificate? / Kenapa bukan sertifikat EV?

**EN:** EV (Extended Validation) code-signing certificates are only issued to
registered companies, cost several hundred USD per year, and still require
accumulated reputation for SmartScreen. CodeForge is a free, open-source,
community project — instead of paying a certificate authority, we sign with a
transparent project certificate, publish it with every release, and document
every way to verify authenticity. For an open-source app, the repository and
checksums are the strongest trust anchors.

**ID:** Sertifikat EV hanya diterbitkan untuk perusahaan terdaftar, biayanya
ratusan dolar per tahun, dan tetap butuh reputasi untuk SmartScreen. CodeForge
adalah proyek open source gratis — kami memakai sertifikat proyek yang
transparan, dipublikasikan di setiap rilis, dengan panduan verifikasi lengkap.
Untuk aplikasi open source, repositori + checksum adalah jangkar kepercayaan
terkuat.

# Monitor Tegangan Baterai INA219 — ESP32 + INA219 + LCD I2C

Aki 12 volt tinggal 9 volt, motormu mogok mendadak. Alat kecil ini
menjaga supaya itu tidak terjadi diam-diam: modul INA219 mengukur
tegangan, arus, dan daya aki, ESP32 menghitungnya, dan hasilnya tampil
di LCD — tegangan, arus, daya, dan persen sisa aki, diperbarui tiap
2 detik. Begitu tegangan turun di bawah 11 volt, buzzer berbunyi
putus-putus dan LED merah menyala sebagai peringatan bahwa aki sudah
lemah dan starter bisa gagal.

Video penjelasan (60 detik):
link menyusul setelah upload

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Modul INA219 (alamat I2C 0x40) | Mengukur tegangan bus, arus, dan daya; shunt 0,1 ohm di modul |
| LCD 16x2 I2C (alamat 0x27) | Menampilkan tegangan + arus dan daya + persen sisa aki |
| Buzzer aktif | Berbunyi putus-putus saat tegangan di bawah 11 volt |
| LED merah + resistor 220 ohm | Menyala saat tegangan rendah (di bawah 11 volt) |
| LED hijau + resistor 220 ohm | Menyala saat tegangan normal (11 volt ke atas) |
| Aki 12 V timbal-asam | Yang diukur — mis. aki motor atau aki mobil |
| Beban 12 V | Lampu 12 V, supaya ada arus yang mengalir untuk diukur |
| Catu USB 5V | Adaptor atau port laptop, untuk ESP32 + LCD |

## Wiring

| Dari | Ke |
|---|---|
| Aki 12 V (+) | INA219 VIN+ |
| INA219 VIN- | Beban 12 V (+) |
| Beban 12 V (-) | Aki 12 V (-) |
| INA219 VCC | ESP32 3V3 |
| INA219 GND | GND |
| INA219 SDA | ESP32 GPIO21 |
| INA219 SCL | ESP32 GPIO22 |
| LCD SDA | ESP32 GPIO21 (bus I2C yang sama dengan INA219) |
| LCD SCL | ESP32 GPIO22 (bus I2C yang sama dengan INA219) |
| LCD VCC | 5V |
| LCD GND | GND |
| Buzzer + | ESP32 GPIO26 (aktif HIGH) |
| Buzzer - | GND |
| LED merah anoda | ESP32 GPIO14, lewat resistor 220 ohm ke GND |
| LED hijau anoda | ESP32 GPIO12, lewat resistor 220 ohm ke GND |
| USB 5V | ESP32 VIN (lewat konektor USB board) |
| Semua GND | Digabung (aki -, INA219, LCD, buzzer, LED, USB -) |

Skema lengkap: [schematic.png](schematic.png)

PERHATIAN: aki 12 V HANYA masuk ke VIN+ / VIN- modul INA219 — JANGAN
pernah menyambung aki 12 V ke pin ESP32, board akan rusak. INA219
mengukur arus dari selisih tegangan di resistor shunt antara VIN+ dan
VIN-, dan tegangan bus di sisi VIN- terhadap GND. Tanpa GND bersama
(aki - digabung dengan GND ESP32), pembacaan tegangan bus tidak ada
acuannya. ESP32 + LCD dicatu USB 5 V terpisah.

## Cara kerja

INA219 duduk di jalur antara aki dan beban: arus dari aki masuk lewat
VIN+, keluar lewat VIN- menuju beban, lalu kembali ke aki -. Di dalam
modul ada resistor shunt 0,1 ohm; INA219 mengukur beda tegangan di
shunt itu untuk menghitung arus, mengukur tegangan bus, dan mengalikan
keduanya menjadi daya — ketiganya dibaca ESP32 lewat I2C.

Setiap 2 detik, sketch membaca tegangan bus (volt), arus (miliampere),
dan daya (miliwatt) dari INA219, lalu menghitung persen sisa aki
secara linear: 10,5 volt dianggap 0% (aki kosong) dan 12,7 volt
dianggap 100% (aki penuh). Ini perkiraan kasar untuk aki timbal-asam
12 V — bukan pengukuran kapasitas sebenarnya — tapi cukup untuk tahu
aki masih sehat atau sudah lemah.

LCD baris pertama menampilkan tegangan dan arus, baris kedua
menampilkan daya dan persen sisa aki. Begitu tegangan turun di bawah
11,0 volt, alarm aktif: buzzer berbunyi putus-putus (300 ms bunyi,
700 ms diam, tanpa delay yang menghentikan loop), LED merah menyala,
dan LED hijau padam. Di atas ambang itu LED hijau menyala dan LED
merah padam. Semua angka juga dicetak ke Serial Monitor (115200 baud)
tiap 2 detik.

Kalau INA219 tidak ditemukan di bus I2C saat start, LCD menampilkan
"INA219 hilang!" dan sketch berhenti — cek wiring SDA/SCL dan alamat
modul (0x40, bisa diubah dengan jumper A0/A1 di modul).

## Konfigurasi (di awal sketch)

- `AMBANG_ALARM_V` (11.0) — di bawah tegangan ini aki dianggap lemah.
- `TEGANGAN_PENUH_V` (12.7) — tegangan aki penuh, 100%.
- `TEGANGAN_KOSONG_V` (10.5) — tegangan aki kosong, 0%.
- `INTERVAL_BACA_MS` (2000) — baca INA219 tiap 2 detik.
- `PERIODE_BEEP_MS` (1000) dan `DURASI_BEEP_ON_MS` (300) — pola bunyi buzzer.
- `ALAMAT_INA219` (0x40) — alamat I2C modul INA219.
- `ALAMAT_LCD` (0x27) — alamat I2C LCD 16x2.

Kalibrasi 32 V / 2 A dipakai di sketch (`setCalibration_32V_2A`) —
cocok untuk aki 12 V (maks 32 volt, maks 2 ampere). Rentang 2 ampere
adalah batas untuk shunt 0,1 ohm bawaan modul — kalau bebanmu menarik
arus lebih dari 2 ampere, ganti resistor shunt modul dengan nilai yang
lebih kecil (dan hitung ulang kalibrasinya) atau pakai modul INA219
dengan shunt yang sesuai. Pantau lewat Serial Monitor (115200 baud): tiap 2 detik
tegangan, arus, daya, persen sisa, dan status alarm dicetak.

## Library yang dibutuhkan

- Adafruit INA219 1.2.3 (butuh Adafruit BusIO + Adafruit Unified Sensor).
- LiquidCrystal_I2C 1.1.2 (LCD 16x2 via backpack PCF8574).

Library ada di folder `libraries/` (file ZIP, folder library di
top-level zip) — pasang lewat Arduino IDE (Sketch > Include Library >
Add .ZIP Library) atau salin ke folder libraries Arduino.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 MonitorTeganganBateraiINA219
```

Terverifikasi 24 Nov 2026: sketch 323.513 byte (24% flash),
RAM 24.752 byte (7%), ESP32 core 4.0.0, arduino-cli 1.6.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).

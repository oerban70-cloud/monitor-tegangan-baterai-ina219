// MonitorTeganganBateraiINA219.ino — Monitor tegangan aki 12 V berbasis ESP32.
//
// Cara kerja:
//   1. Modul INA219 mengukur tegangan bus (sisi beban), arus yang mengalir
//      lewat resistor shunt 0,1 ohm di modul, dan daya — semuanya lewat I2C.
//      Rentang kalibrasi 32 V / 2 A, cukup untuk aki 12 V.
//   2. Setiap INTERVAL_BACA_MS (2 detik), ESP32 membaca tegangan (volt),
//      arus (miliampere), dan daya (miliwatt) dari INA219, lalu menghitung
//      persen sisa aki secara linear antara TEGANGAN_KOSONG_V (0%) dan
//      TEGANGAN_PENUH_V (100%).
//   3. LCD menampilkan tegangan + arus di baris pertama, daya + persen sisa
//      aki di baris kedua.
//   4. Bila tegangan turun di bawah AMBANG_ALARM_V (11,0 V — aki 12 V sudah
//      lemah dan starter bisa gagal), buzzer berbunyi putus-putus, LED merah
//      menyala, dan LED hijau padam. Di atas ambang itu LED hijau menyala.
//
// Pinout (HARUS sama dengan skema di schematic.png):
//   GPIO21 -> INA219 SDA dan LCD SDA (bus I2C yang sama)
//   GPIO22 -> INA219 SCL dan LCD SCL (bus I2C yang sama)
//   GPIO26 -> buzzer aktif HIGH
//   GPIO14 -> LED merah (tegangan rendah) lewat resistor 220 ohm ke GND
//   GPIO12 -> LED hijau (tegangan normal) lewat resistor 220 ohm ke GND
//
// Jalur daya aki (HARUS sama dengan skema):
//   Aki 12 V (+) -> INA219 VIN+ ; INA219 VIN- -> beban 12 V -> aki (-).
//   INA219 mengukur arus dari selisih tegangan di resistor shunt antara
//   VIN+ dan VIN-, dan tegangan bus di sisi VIN- terhadap GND.
//   GND INA219 dan GND ESP32 digabung dengan aki (-) — tanpa GND bersama,
//   pembacaan tegangan bus tidak ada acuannya.
//   ESP32 + LCD dicatu USB 5 V terpisah; JANGAN mencatu ESP32 dari aki
//   12 V tanpa regulator penurun tegangan.
//
// Library yang dibutuhkan (lihat folder libraries/ di repo):
//   - Adafruit INA219 1.2.3 (butuh Adafruit BusIO + Adafruit Unified Sensor)
//   - LiquidCrystal_I2C 1.1.2
//
// Catu daya: ESP32 + LCD dari USB 5 V. INA219 VCC dari 3V3 ESP32.
// Aki 12 V HANYA masuk ke VIN+ / VIN- modul INA219, bukan ke pin ESP32.

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

// -------------------- pin --------------------
const uint8_t PIN_BUZZER    = 26;  // aktif HIGH
const uint8_t PIN_LED_MERAH = 14;  // tegangan rendah, lewat resistor 220 ohm
const uint8_t PIN_LED_HIJAU = 12;  // tegangan normal, lewat resistor 220 ohm
const uint8_t PIN_SDA       = 21;  // bus I2C: INA219 + LCD
const uint8_t PIN_SCL       = 22;  // bus I2C: INA219 + LCD

// -------------------- konfigurasi --------------------
const float         AMBANG_ALARM_V      = 11.0f;  // di bawah ini aki lemah
const float         TEGANGAN_PENUH_V    = 12.7f;  // aki 12 V penuh (100%)
const float         TEGANGAN_KOSONG_V   = 10.5f;  // aki 12 V kosong (0%)
const unsigned long INTERVAL_BACA_MS    = 2000;   // baca INA219 tiap 2 detik
const unsigned long PERIODE_BEEP_MS     = 1000;   // satu siklus bunyi buzzer
const unsigned long DURASI_BEEP_ON_MS   = 300;    // buzzer bunyi 300 ms tiap siklus
const uint8_t       ALAMAT_INA219       = 0x40;   // alamat I2C modul INA219
const uint8_t       ALAMAT_LCD          = 0x27;   // alamat I2C LCD 16x2
const uint8_t       KOLOM_LCD           = 16;
const uint8_t       BARIS_LCD           = 2;
const uint32_t      KECEPATAN_SERIAL    = 115200;

// -------------------- objek --------------------
Adafruit_INA219 ina219(ALAMAT_INA219);
LiquidCrystal_I2C lcd(ALAMAT_LCD, KOLOM_LCD, BARIS_LCD);

// -------------------- state --------------------
float teganganVolt = 0.0f;   // tegangan bus aki (volt)
float arusMiliampere = 0.0f; // arus beban (mA)
float dayaMiliwatt = 0.0f;   // daya beban (mW)
float persenSisaAki = 0.0f;  // 0-100%
bool alarmAktif = false;
unsigned long waktuBacaTerakhirMs = 0;

// -------------------- persen sisa aki --------------------
// Linear antara TEGANGAN_KOSONG_V (0%) dan TEGANGAN_PENUH_V (100%).
// Ini perkiraan kasar untuk aki timbal-asam 12 V — bukan pengukuran
// kapasitas sebenarnya, tapi cukup untuk tahu aki masih sehat atau tidak.
float hitungPersenSisaAki(float volt) {
  if (volt <= TEGANGAN_KOSONG_V) return 0.0f;
  if (volt >= TEGANGAN_PENUH_V) return 100.0f;
  return (volt - TEGANGAN_KOSONG_V)
         / (TEGANGAN_PENUH_V - TEGANGAN_KOSONG_V) * 100.0f;
}

// -------------------- buzzer --------------------
// Bunyi putus-putus non-blocking: 300 ms bunyi, 700 ms diam, berulang
// selama alarm aktif. Dipanggil tiap loop, tidak memakai delay().
void perbaruiBuzzer() {
  if (!alarmAktif) {
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }
  unsigned long faseMs = millis() % PERIODE_BEEP_MS;
  digitalWrite(PIN_BUZZER, faseMs < DURASI_BEEP_ON_MS ? HIGH : LOW);
}

// -------------------- LCD --------------------
void tampilkanLcd() {
  char baris1[17];
  char baris2[17];
  snprintf(baris1, sizeof(baris1), "%.2fV %.0fmA",
           teganganVolt, arusMiliampere);
  snprintf(baris2, sizeof(baris2), "%.1fW %d%%",
           dayaMiliwatt / 1000.0f, (int)persenSisaAki);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
}

// -------------------- baca sensor + alarm --------------------
void bacaSensorDanPerbaruiAlarm() {
  teganganVolt = ina219.getBusVoltage_V();
  arusMiliampere = ina219.getCurrent_mA();
  dayaMiliwatt = ina219.getPower_mW();
  persenSisaAki = hitungPersenSisaAki(teganganVolt);

  alarmAktif = teganganVolt < AMBANG_ALARM_V;
  digitalWrite(PIN_LED_MERAH, alarmAktif ? HIGH : LOW);
  digitalWrite(PIN_LED_HIJAU, alarmAktif ? LOW : HIGH);

  tampilkanLcd();

  Serial.print("Tegangan: ");
  Serial.print(teganganVolt, 2);
  Serial.print(" V | Arus: ");
  Serial.print(arusMiliampere, 0);
  Serial.print(" mA | Daya: ");
  Serial.print(dayaMiliwatt / 1000.0f, 1);
  Serial.print(" W | Sisa: ");
  Serial.print((int)persenSisaAki);
  Serial.print("% | Alarm: ");
  Serial.println(alarmAktif ? "YA (aki lemah)" : "tidak");
}

// -------------------- setup / loop --------------------
void setup() {
  Serial.begin(KECEPATAN_SERIAL);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_MERAH, LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Monitor Aki 12V");
  lcd.setCursor(0, 1);
  lcd.print("INA219 siap...");

  // Kalibrasi 32 V / 2 A — cocok untuk aki 12 V (maks 32 V, maks 2 A).
  if (!ina219.begin()) {
    Serial.println("INA219 tidak ditemukan di I2C! Cek wiring SDA/SCL.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("INA219 hilang!");
    lcd.setCursor(0, 1);
    lcd.print("Cek SDA/SCL");
    while (true) {
      delay(1000);  // berhenti: tanpa INA219 tidak ada yang bisa diukur
    }
  }
  ina219.setCalibration_32V_2A();
  Serial.println("INA219 siap. Membaca tiap 2 detik.");
  delay(1000);
}

void loop() {
  unsigned long sekarang = millis();
  if (sekarang - waktuBacaTerakhirMs >= INTERVAL_BACA_MS) {
    waktuBacaTerakhirMs = sekarang;
    bacaSensorDanPerbaruiAlarm();
  }
  perbaruiBuzzer();
}

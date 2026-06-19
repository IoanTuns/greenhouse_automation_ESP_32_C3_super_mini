# Context Proiect: Automatizare Solar Inteligent cu MicroSD (Phase 1)

Acest document servește ca instrucțiune de bază și context pentru orice Agent AI sau sesiune de programare viitoare. AI-ul trebuie să respecte cu strictețe arhitectura hardware și structura software descrise mai jos în orice propunere de cod sau modificare.

## 1. Arhitectură și Constrângeri Principale
- **Microcontroler:** ESP32-C3 Super Mini (Arhitectură RISC-V).
- **Mediu de dezvoltare:** VS Code cu extensia PlatformIO (Framework Arduino).
- **Stocare Cod:** GitHub Repository.
- **Mod Funcționare:** Complet OFFLINE. Placa generează propriul Wi-Fi local (Access Point) și găzduiește o pagină web locală la `192.168.4.1` pentru monitorizare de pe telefon.
- **Data Logging:** Salvează periodic în fișierul `/date_solar.csv` de pe cardul MicroSD (formatat FAT32) datele de la toți senzorii și volumul total pompat.
- **Logică Automatizare:** Mașină de stări (State Machine) pentru irigare secvențială bazată pe volumul de apă (debitmetre), declanșată la o oră fixă din RTC.

## 2. Configurație Hardware & Mapare Pini (`include/config_hardware.h`)
Toți pinii fizici sunt mapați prin constante și TREBUIE apelați în cod prin numele variabilelor de mai jos:

- `PIN_SDA` (GPIO 1) & `PIN_SCL` (GPIO 0) -> Comunicație I2C pentru Ceasul RTC DS3231.
- `PIN_SD_MISO` (GPIO 2), `PIN_SD_MOSI` (GPIO 3), `PIN_SD_SCK` (GPIO 4), `PIN_SD_CS` (GPIO 10) -> Magistrală SPI dedicată modulului de card MicroSD. GPIO 2 este strapping pin (HIGH = boot normal) — are rezistență fizică pull-up de 10kΩ la 3.3V pe linia MISO.
- `PIN_POMPA` (GPIO 5) -> Releu 1 (Pompă apă 220V AC).
- `PIN_VALVA_Z1` (GPIO 8) -> Releu 2 (Electrovalvă Zona 1 - 12V DC).
- `PIN_VALVA_Z2` (GPIO 9) -> Releu 3 (Electrovalvă Zona 2 - 12V DC).
- `PIN_Senzori_Temp` (GPIO 6) -> Magistrală One-Wire pentru doi senzori waterproof DS18B20 (Pământ și Apă). Necesită rezistență pull-up de 4.7kΩ la 3.3V.
- `PIN_DHT22` (GPIO 7) -> Senzor Digital pentru Climatul Aerului (Temperatură + Umiditate aer). Necesită rezistență fizică pull-up de 10kΩ la 3.3V pe linia DATA.
- `PIN_DEBIT_Z1` (GPIO 20) -> Debitmetru Zona 1 (YF-S201). Folosește întrerupere hardware (`RISING`). Senzorul este alimentat la 5V, iar semnalul de ieșire este 5V — pe linia de semnal există un divizor de tensiune fizic (R1=10kΩ, R2=20kΩ la GND) care coboară semnalul la 3.33V înainte de GPIO.
- `PIN_DEBIT_Z2` (GPIO 21) -> Debitmetru Zona 2 (YF-S201). Folosește întrerupere hardware (`RISING`). Același divizor de tensiune ca la Zona 1.

**Logică Relee:** Modulele de relee folosite sunt active pe `LOW` (`0V` = Pornit, `3.3V/5V` = Oprit). În cod se folosesc constantele `RELEU_PORNIT` și `RELEU_OPRIT`. Fizic: jumperul VCC↔JD-VCC este eliminat — JD-VCC este alimentat la 5V (bobine relee) și VCC la 3.3V (logică/optocuploare). Pe fiecare linie IN (GPIO 5, 8, 9) există o rezistență serie de 1kΩ față de pinul ESP32-C3.

## 3. Parametri Sistem & Setări (`include/config_sistem.h`)
- `WIFI_SSID` = "Solar_Inteligent"
- `WIFI_PASSWORD` = "automatizare_solar"
- `ORA_PORNIRE` & `MINUT_PORNIRE` -> Declanșarea zilnică a irigării.
- `VOLUM_TINTA_Z1` & `VOLUM_TINTA_Z2` -> Volumul de apă în litri alocat fiecărei zone (implicit 50.0L).
- `IMPULSURI_PER_LITRU` = 450.0 -> Factorul de calibrare pentru debitmetrul YF-S201.

## 4. Logica Mașinii de Stări (State Machine)
Sistemul navighează prin trei stări (`enum StareUdare`):
1. `OPRIT`: Monitorizează senzorii și așteaptă ca RTC-ul să atingă ora de pornire.
2. `UDARE_ZONA_1`: Închide valva 2, deschide valva 1 și pornește pompa. Numără impulsurile de la debitmetrul 1 până când se atinge volumul țintă.
3. `UDARE_ZONA_2`: Închide valva 1, deschide valva 2 (pompa rămâne pornită). Numără impulsurile de la debitmetrul 2. La finalizarea volumului, oprește tot, salvează un raport imediat pe cardul SD și revine în starea `OPRIT`.
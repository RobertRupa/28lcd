# ES3C28P – LVGL Peripheral Lab

Projekt do otwarcia w **VS Code + PlatformIO na Ubuntu**. Bazuje na mapowaniu LCDWiki i rozwiązaniach `paeqe/es3c28p-dap` (LovyanGFX, LVGL 8, odwrócenie osi dotyku, inicjalizacja ES8311). **Nie jest kopią kompletnego odtwarzacza muzyki.**

## Funkcje

| Peryferium | Stan w projekcie |
|--|--|
| LCD ILI9341V SPI | inicjalizacja, LVGL, suwaki PWM jasności |
| FT6336G I²C | dotyk w LVGL, wykrywanie przez I²C |
| ES8311 + FM8002E | wykrywanie, inicjalizacja DAC i test tonu 440 Hz przez I²S (przycisk TESTY) |
| Mikrofon analogowy przez ES8311 ADC | **niezaimplementowany odczyt próbek**; codec współdzieli I²S |
| SD microSD | SD_MMC w trybie 4-bit, wykrywanie i lista plików |
| RGB adresowalny | cykliczne kolory przyciskiem GUI |
| GPIO0 BOOT | raport stanu w GUI |
| ADC baterii IO9 | surowa wartość + szacunek napięcia; kalibracja dzielnika wymagana |
| Wi-Fi | skanowanie SSID/RSSI z GUI |
| BLE | skanowanie BLE, lista adresów i RSSI na ekranie |
| UART0 / I²C / GPIO ekspansji | pinout udokumentowany; tylko I²C skanowany, pozostałe nie są zajmowane |
| USB CDC / PSRAM | log i diagnostyka |

**Ważne:** wszystkie peryferia nie mogą być „włączone” bez warunków: np. wyjścia GPIO2/3/14/21 powinny pozostać wolne do podłączenia urządzenia zewnętrznego. Mikrofon wymaga konfiguracji ADC ES8311 (projekt nie udaje, że go nagrywa).

## Instalacja Ubuntu

```bash
sudo apt update
sudo apt install -y git python3 python3-venv python3-pip build-essential udev
python3 -m venv ~/.venvs/platformio
~/.venvs/platformio/bin/pip install -U platformio
cd ES3C28P_LVGL_Peripherals
~/.venvs/platformio/bin/pio run
~/.venvs/platformio/bin/pio run -t upload --upload-port /dev/ttyACM0
~/.venvs/platformio/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

W VS Code zainstaluj rozszerzenie **PlatformIO IDE**, wybierz **File > Open Folder**, otwórz folder projektu, użyj **Build / Upload / Monitor**. Pierwsza kompilacja pobiera biblioteki z internetu. Jeżeli port ma inną nazwę, sprawdź `ls -l /dev/serial/by-id/` lub `pio device list`.

## Bezpieczeństwo i uwagi

- **Głośnik**: podłącz do złącza SPEAKER, przed kliknięciem `Test glosnika` sprawdź głośność. Dla niepodłączonego głośnika kliknięcie niczego pożytecznego nie testuje.
- **Akumulator LiPo**: zachowaj biegunowość złącza BAT i używaj zgodnego 3,7 V; napięcie na IO9 wymaga potwierdzenia współczynnika dzielnika przy użyciu multimetru. Gwiazdka w odczycie oznacza wartość orientacyjną, nie pomiar kalibrowany.
- **GPIO45/GPIO46** to piny strappingowe ESP32-S3; nie zmieniaj ich funkcji podczas startu.
- Peryferia I²C FT6336G (`0x38`) i ES8311 (`0x18`) współdzielą SDA16/SCL15. Przycisk I²C skanuje magistralę i raportuje w monitorze szeregowym.
- Podświetlenie i RGB uruchamiane są przy starcie, inne testy po dotknięciu przycisku.
- Dźwięk 440 Hz jest testem wyjścia DAC, nie sprawdza ścieżki mikrofonowej.
- Brak testu na fizycznej płytce i brak wykonanej tu kompilacji PlatformIO (środowisko nie ma sieci). Przed użyciem produkcyjnym wymagany jest build i test na ES3C28P.

## Źródła
- https://github.com/paeqe/es3c28p-dap
- https://www.lcdwiki.com/2.8inch_ESP32-S3_Display


## LVGL: termostat pokojowy i panel administracyjny

Firmware uruchamia cztery zakladki (240x320, LVGL 8):

- **POKOJ** — ekran glowny: temperatura symulowana, temperatura zadana 15–30°C i logiczny status ogrzewania.
- **ADMIN** — wlacznik trybu AUTO, histereza 0.2–2.0°C, suwak symulacji temperatury 10–35°C, raport peryferiow i odczyt ADC baterii.
- **TESTY** — WS2812, podswietlenie, ton 440 Hz na ES8311, skanowanie I2C.
- **SD/WIFI** — lista plikow microSD, skan sieci Wi-Fi i BLE.

Nastawa, histereza i tryb zapisywane sa przez Arduino Preferences w NVS (namespace `thermostat`).
Histereza działa jako dwupoziomowy regulator on/off: zalaczenie przy `temperatura <= cel - histereza/2`, wylaczenie przy `temperatura >= cel + histereza/2`.

**Bezpieczenstwo:** temperaturę dostarcza obecnie wylacznie suwak symulacyjny. Program **nie steruje przekaznikiem, triakiem ani innym wyjsciem fizycznym**. Nie uzywac firmware jako rzeczywistego termostatu grzewczego bez podlaczenia i kalibracji czujnika, przypisania wyjscia i implementacji bezpiecznego stanu awaryjnego.

### Budowa na Ubuntu / VS Code

```bash
python3 -m venv ~/.venvs/platformio
~/.venvs/platformio/bin/pip install -U platformio
git clone https://github.com/RobertRupa/28lcd.git
cd 28lcd
~/.venvs/platformio/bin/pio run
~/.venvs/platformio/bin/pio run -t upload --upload-port /dev/ttyACM0
~/.venvs/platformio/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

Po otwarciu katalogu w VS Code mozna uzyc rozszerzenia PlatformIO. Projekt ma zadeklarowane LVGL 8, LovyanGFX i Adafruit NeoPixel.

Ograniczenia: kompilacja i test na fizycznej plytce nie zostaly przeprowadzone w ramach tej aktualizacji.


## Dashboard LVGL — gałąź `feature/lvgl-dashboard`

Ta gałąź przebudowuje interfejs na 7 widoków LVGL 8: DOM, RADIO, AUDIO, LCD, BAT, RGB, ADMIN.
Nowe widoki: `src/ui_dashboard.h`. Zachowane działające sterowniki ILI9341, FT6336, ES8311, SD_MMC i WS2812 z projektu bazowego.

### Status funkcjonalny (nie mylić GUI z kompletnym sterownikiem)

| Funkcja | Status |
|---|---|
| Termostat, nastawa/histereza NVS | Zaimplementowano demonstrację, bez przekaźnika |
| DHT11/DHT22 | Opcjonalne odczyty na GPIO2, po konfiguracji build |
| Wi-Fi SSID + hasło w NVS | Dodano UI, zapis, ponowne łączenie i usuwanie |
| Skan Wi-Fi/BLE | Zachowane; operacje blokujące GUI na czas skanowania |
| LCD i dotyk | Regulacja jasności, pole testowe z koordynatami |
| WS2812 | Zachowana zmiana kolorów |
| ES8311 | Zachowany test 440 Hz |
| SD | Zachowana lista plików |
| Nagranie WAV / odtworzenie WAV / MP3 | **Niezaimplementowane** — przyciski informacyjne |
| Bateria | Szacowany odczyt ADC, wymagana kalibracja dzielnika |
| Thread/Zigbee ESP32-C5/C6/H2 | **Niezaimplementowane** — wymagane połączenie RCP i protokół |
| CCTV MJPEG/RTSP | **Niezaimplementowane** — wymagany transport, dekoder i wydajnościowe testy |

### Opcjonalny czujnik temperatury DHT

Domyślnie `DHT_GPIO=-1`: termostat używa temperatury symulowanej.
Aby przełączyć na fizyczny czujnik, dodaj do `build_flags` w `platformio.ini`:

```ini
  -DDHT_GPIO=2
  -DDHT_SENSOR_TYPE=22
```

Dla DHT11 ustaw `-DDHT_SENSOR_TYPE=11`. DHT DATA podłącz do **GPIO2**, masa do GND, zasilanie zgodne z modułem (zalecane 3.3 V), w razie potrzeby rezystor podciągający DATA do 3.3 V. Przy błędzie odczytu DHT zapotrzebowanie na grzanie w logice jest zerowane. **Nie jest to gotowe urządzenie bezpieczeństwa cieplnego.**

### Koprocesor Thread / Zigbee

ESP32-S3 nie posiada radia 802.15.4. Do Thread/Zigbee potrzebny jest zewnętrzny **ESP32-C5/C6/H2**.
Na płytce dostępne są np. GPIO43 (RX0) i GPIO44 (TX0), ale współdzielą one UART0 — zanim podłączysz RCP, skonfiguruj konsolę USB i sprawdź bootlog oraz poziomy 3.3 V. Sama wymiana ramek UART nie implementuje OpenThread Border Routera: potrzebny jest Spinel RCP / warstwa hosta i odpowiednia integracja sieci IP.
Wykonywanie poleceń termostatu na zdalnym przekaźniku powinno wymagać ACK, watchdog i timeout wyłączający przekaźnik, przy braku danych/łączności.

### Ograniczenia

- Hasło Wi-Fi jest przechowywane w NVS przez `Preferences` — **nie gwarantuje to szyfrowania flash**. W urządzeniu produkcyjnym należy rozważyć NVS encryption / flash encryption.
- Nie dokonano kompilacji PlatformIO ani testu na płytce w ramach tych zmian.
- Na LCD 240x320 zawartość widoków przewija się w pionie; pasek zakładek przewija się w poziomie.
- UI zostaje na LVGL **8.3**, a nie 9.x — to świadomy etap migracji bez zmiany sprawdzonych sterowników.

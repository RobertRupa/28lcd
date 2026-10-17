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

# 28LCD thermostat — local WebGUI and planned Matter

## Current implementation
The ESP32-S3 is a Wi-Fi HTTP client/server, **not yet a commissioned Matter device**.
Open `http://<LCD-IP>/` using the existing admin Basic Auth credentials.
The same page retains gate/camera configuration and adds:
- Setpoint (15-30 °C), hysteresis (0.2-2.0 °C), AUTO/OFF, persisted in thermostat NVS.
- WS2812 LED gradient based on **setpoint**, blue at 14 °C, red at 28 °C by default; configurable endpoints and brightness saved in NVS.
- Name of the planned Matter device. This does not enable Matter commissioning.
- Live HTTP status including estimated battery voltage.
Authenticated `GET /status` returns `temperature`, `source`, `heating`, `target`, `hysteresis`, `enabled`, `battery_voltage_estimate`, `battery_percent=null`, `charging=null`, `usb_power=null`, `led_blue_temp`, `led_red_temp`, `ip`.

The battery ADC estimate on GPIO9 must be calibrated against a multimeter and the board's actual divider. Charge state / percentage are unknown, not guessed. Without an external DHT sensor, the chip temperature is **not ambient** and cannot enable heating. No heating relay is configured.

## Matter commissioning (future firmware)
The 11-digit code is the **Matter manual pairing code**, derived from the actual Matter commissioning payload; it is not a Wi-Fi password, OpenThread dataset, nor a user-chosen static number. A real ESP-Matter Thermostat endpoint, factory/onboarding credentials, commissioning window and pairing QR must be added to firmware before pairing with a Matter controller (for example Home Assistant / Google Home). The existing Thread BR is unnecessary for Matter-over-Wi-Fi transport. UniFi Network listing the ESP32 as a Wi-Fi client alone does not commission Matter.
See https://docs.espressif.com/projects/esp-matter/en/latest/esp32s3/faq.html

## Example
```bash
curl -u admin http://<LCD-IP>/status
```
The HTTP panel uses Basic Auth over unencrypted HTTP. Restrict access to trusted LAN; never expose directly to the internet.

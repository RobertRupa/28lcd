# ESP32-S3 Matter Thermostat — implementation requirements

Status: **not implemented**. Do not advertise Matter credentials or a pairing code before a functioning CHIP/Matter node exists.

The existing PlatformIO target uses `platform=espressif32@6.9.0`, `framework=arduino`. This is not a tested ESP-Matter build. ESP-Matter uses ESP-IDF and CHIP; migrating to an ESP-IDF project with Arduino as a component may be required to preserve LovyanGFX/LVGL and other peripherals. The current HTTP status, Wi-Fi, gate, and camera functionality is not a substitute for Matter.

## Required stages
1. Create a separate ESP-IDF / ESP-Matter build target for ESP32-S3 with the correct flash/PSRAM and partition settings; do not disrupt the current working PlatformIO firmware.
2. Bring up a real Matter Thermostat endpoint / cluster (current temperature, occupied heating setpoint, system mode, running state). Map writes to the existing NVS and LVGL, and map device measurements back to attributes.
3. Enable Wi-Fi network commissioning with BLE or appropriate on-network commissioning, IPv6 and operational mDNS. Avoid conflicting HTTP and Matter networking stacks.
4. Provision an allowed setup passcode, discriminator, device attestation for a test/development configuration. Generate the 11-digit *manual pairing code* and QR from exactly those live commissioning parameters; never hard-code an unrelated random 11-digit number. Do not publish a production secret in Git.
5. Add WebGUI/LCD commissioning control and state reporting, with explicit indications of whether a commissioning window is open and whether the device joined a fabric. Protect administrative actions.
6. Verify pairing, reconnect after reset, attribute reads/writes and multiple power cycles against a real Matter controller, e.g. Home Assistant/Google Home.

The standalone Thread border router is **not** the Matter controller for a Wi-Fi Matter device. In UniFi Network, the ESP32-S3 remains a Wi-Fi client.

Official sources:
- https://docs.espressif.com/projects/esp-matter/en/latest/esp32s3/faq.html
- https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html

## SUPLA Direct HTTPS
The existing WebGUI accepts private IPv4 HTTP and HTTPS only for `svr34.supla.org`. TLS certificate validation requires the user to provide a trusted CA PEM in the new HTTPS section. Connections fail closed when it is not configured. An individual SUPLA Direct URL may contain a secret access token: do not print or publicly share URLs.


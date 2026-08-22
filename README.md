# ESP32 Share CGM Desktop Monitor

An unofficial ESP32 desktop monitor that can display glucose data from a
Dexcom G7 CGM system through Dexcom Share.

Firmware **V3.1.0** targets the Hosyond/LCDWiki E32R35T desktop display:
ESP32-WROOM-32E, 480x320 ST7796 TFT, XPT2046 resistive touch, and onboard
microSD.

This is an independent community project. It is not affiliated with, authorized
by, sponsored by, or endorsed by Dexcom, Inc. The monitor retrieves readings
through an unofficial/private Dexcom Share interface over Wi-Fi and does not
communicate directly with a sensor over Bluetooth. That interface may change or
stop working without notice.

This project is a secondary informational display only. It is not a medical
device and is not a replacement for an approved CGM application or receiver,
medical alarms, professional advice, or treatment decisions. Confirm unexpected
readings and alerts using the approved CGM workflow.

## V3.1.0 features

- live glucose value with Dexcom-provided trend arrow and description;
- color-coded glucose: red below 70, green from 70–180, and yellow above 180 mg/dL;
- touch-selectable 3H, 6H, 12H, and 24H TFT graphs with the last range
  restored after reboot;
- 70, 180, and 250 mg/dL graph references with flashing critical alerts below
  65 or above 250 mg/dL;
- persistent daily CSV logging to the onboard microSD slot;
- recovery of recent graph history after reboot;
- responsive web dashboard with live glucose, statistics, and 3H–30D history;
- streamed, downsampled 7D and 30D requests with bounded ESP32 memory use;
- CSV export for today, seven days, or a validated date range;
- captive-portal Wi-Fi and Dexcom publisher-account provisioning;
- configurable POSIX timezone, 12/24-hour clock, and screen-off schedule;
- TFT and web controls for clock format and four-point touch recalibration;
- graceful Wi-Fi, Dexcom, and SD failure recovery.

## Hardware mapping

| Function | ESP32 pin / bus |
|---|---:|
| TFT CS | GPIO 15, HSPI |
| TFT DC | GPIO 2 |
| TFT SCLK | GPIO 14 |
| TFT MOSI | GPIO 13 |
| TFT MISO | GPIO 12 |
| TFT backlight | GPIO 27 |
| Touch CS | GPIO 33, HSPI |
| Touch IRQ | GPIO 36 |
| SD CS | GPIO 5, VSPI |
| SD SCK | GPIO 18 |
| SD MISO | GPIO 19 |
| SD MOSI | GPIO 23 |

## Build and upload

Requirements: VS Code with PlatformIO, or PlatformIO Core.

```powershell
cd "PATH\TO\ESP32-Share-CGM-Monitor"
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload --upload-port COM12
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM12 --baud 115200
```

Change `COM12` if PlatformIO reports a different port. Close every serial monitor
before erase or upload operations.

To reproduce the complete first-boot workflow:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t erase --upload-port COM12
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload --upload-port COM12
```

## First boot

1. Complete the four-point touch calibration.
2. Read the welcome and safety screens; each remains visible for two seconds.
3. Join the displayed `CGMMonitor-Setup-XXXXXX` access point.
4. Open `http://192.168.4.1` if the captive page does not appear.
5. Select a 2.4 GHz Wi-Fi network.
6. Enter the Dexcom **publisher/sharer** login and password, not follower credentials.
7. Configure the hostname, timezone, clock, and optional screen schedule.
8. Save and wait for NTP, SD restoration, and Dexcom authentication.

Dexcom Share must be enabled with at least one follower configured. The saved
password is never returned by the dashboard or printed to Serial.

After the monitor joins the local network, open the dashboard at
`http://HOSTNAME.local` (normally `http://esp32-cgm-monitor.local`) or use the
IP address shown on the TFT System Status page. The `.local` address uses mDNS;
client devices or segmented networks without multicast DNS support must use the
numeric IP address.

## SD storage

The independent VSPI microSD interface writes one FAT32-compatible CSV file per
local day:

```text
/dexcom/YYYY/MM/YYYY-MM-DD.csv
```

Stable schema:

```csv
timestamp_iso,epoch_ms,glucose_mgdl,trend,trend_text
```

Epoch milliseconds are the canonical deduplication key. Each new record is
opened, appended, flushed, and closed. The logger warns below 100 MB free and
stops below 20 MB without interrupting glucose monitoring. Existing files are
never automatically deleted in V3.1.0.

## Web API

| Endpoint | Purpose |
|---|---|
| `/` | Responsive dashboard and settings |
| `/status` | Time, current glucose, SD, and non-secret device status |
| `/api/history?range=3h` | History; accepts 3h, 6h, 12h, 24h, 3d, 7d, or 30d |
| `/api/export/today` | Today's CSV download |
| `/api/export?days=7` | Last seven days |
| `/api/export?from=YYYY-MM-DD&to=YYYY-MM-DD` | Validated date-range export |

Short history comes from the bounded 288-reading RAM buffer. Longer history is
streamed from SD; 7D uses 15-minute bins and 30D uses hourly min/average/max bins.

## Security and limitations

- Dexcom Share is an unofficial/private API and may change without notice.
- HTTPS currently uses `WiFiClientSecure::setInsecure()`; service certificate
  validation is the highest-priority future security improvement.
- Wi-Fi and Dexcom setup pages use local HTTP.
- Dexcom credentials are stored in ESP32 NVS. NVS is not encrypted unless ESP32
  flash encryption is separately enabled.
- Retention selections are stored, but V3.1.0 does not automatically delete files.
- The official Share-style retrieval window is at most 24 hours / 288 readings;
  all longer history is accumulated locally on SD.

See [MANUAL.md](MANUAL.md) for operating and troubleshooting instructions.

## License

This project is released under the [MIT License](LICENSE).

The MIT License applies only to this project's original code and documentation.
It does not grant any rights to Dexcom trademarks, branding, software, APIs, or
services. Dexcom and Dexcom G7 are trademarks of Dexcom, Inc.; their names are
used only to describe compatibility and the source of data.

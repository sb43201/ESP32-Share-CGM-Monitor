# ESP32 Share CGM Desktop Monitor V3.1.0 — User Manual

## 1. Safety

This independent project is unofficial and is not affiliated with or endorsed
by Dexcom, Inc. It uses an unofficial/private Dexcom Share interface that may
change without notice. Do not use it as a replacement for an approved CGM app
or receiver, for treatment decisions, or as a medical alarm system. Confirm
unexpected readings and alerts through the approved CGM workflow.

## 2. Normal TFT display

The header shows local weekday/date, time, Wi-Fi state, CGM state, and the
**STATUS** touch target. The main area shows the latest glucose value, units,
Dexcom-provided arrow, and trend description.

Glucose value colors:

- red: below 70 mg/dL, or a reading older than 15 minutes;
- green: 70–180 mg/dL;
- yellow: above 180 mg/dL;
- flashing black on red: below 65 or above 250 mg/dL.

The graph uses a fixed 40–300 mg/dL scale. Reference lines mark 70, 180, and
250 mg/dL. Touch **3H**, **6H**, **12H**, or **24H** to change the range. Tabs
respond on initial contact and use enlarged touch targets. The active tab is
dark teal with a cyan outline and yellow bottom bar. The selected range is saved
and restored after reboot; a device with no saved selection defaults to 3H.

The footer reports the reading age. A stale/offline message does not erase the
last known glucose value.

## 3. First-time setup

After a full flash erase:

1. Touch each of the four calibration targets and release.
2. Read the welcome screen and safety notice. Each displays for two seconds.
3. On a phone or computer, join `CGMMonitor-Setup-XXXXXX`.
4. If needed, browse to `http://192.168.4.1`.
5. Select the 2.4 GHz Wi-Fi network and enter its password.
6. Enter the Dexcom publisher/sharer login and password.
7. Enter the POSIX timezone rule. Indianapolis defaults to
   `EST5EDT,M3.2.0,M11.1.0`.
8. Select 12- or 24-hour time and optional screen-off/on times.
9. Save and allow the portal to close.

The monitor displays its LAN IP in Serial and on the Status screen.

The TFT System Status page also provides **Calibrate touch** and a persistent
**12H CLOCK**/**24H CLOCK** toggle.

## 4. Web dashboard

Open `http://DEVICE-IP/`. The dashboard shows current glucose and age, storage
state, graph, min/max/average, data export, logging controls, and device settings.

The 3H–24H views use recent RAM history. The 3D, 7D, and 30D views depend on data
accumulated by this monitor on SD; Dexcom Share cannot retrospectively fill those
ranges beyond approximately 24 hours.

Use **Calibrate touch** to start calibration on the TFT. Use **Change Wi-Fi** to
erase only saved network credentials and restart the captive portal. Saved
Dexcom credentials remain available. To replace Dexcom credentials, enter only
the fields being changed and save Device settings; passwords are never shown.

## 5. CSV export and SD card

Use **Today**, **Last 7 days**, or select From/To dates. Exports preserve the raw
five-minute CSV records. Date ranges are validated and cannot address arbitrary
SD paths.

The card can be read on a computer as FAT32. Files are under:

```text
/dexcom/year/month/date.csv
```

Do not remove the card while an append is being written. If removal or a write
failure occurs, monitoring continues in RAM and the firmware retries the card
every 60 seconds. On reinsertion, recent RAM readings are backfilled without
duplicating timestamps.

Below 100 MB free, the status shows a warning. Below 20 MB, SD logging stops.
Copy or delete old CSV files on a computer to regain space. V3.1.0 does not
perform automatic retention deletion.

## 6. Screen schedule

Enable Scheduled screen off and choose exact off/on times in the dashboard.
Overnight periods are supported. Only the TFT backlight turns off; Wi-Fi,
Dexcom polling, SD logging, and the web server continue running.

## 7. Status screen

Touch **STATUS** to view IP/RSSI, Dexcom/session state, reading age, NTP, uptime,
history size, heap, SD free space, and sanitized errors. Touch **BACK** to return.
No password, account ID, or full session token is displayed.

## 8. Troubleshooting

### Portal does not appear

Join the setup SSID manually and open `http://192.168.4.1`. The portal times out
after three minutes so the monitor can continue offline. Reboot to retry.

### Authentication fails

Use publisher/sharer credentials, confirm Share is enabled and at least one
follower exists, verify the password, and check that the account is in the US
region supported by the configured endpoint.

### HTTPS request stalls or fails

Check RSSI on the Status screen. Values near -80 dBm are weak; move the monitor
closer to the router. DNS, TLS, and server outages are retried without discarding
the last reading.

### SD is unavailable

Power down, reseat a FAT32 card, and reboot. Monitoring works without a card but
history is then limited to RAM and cannot survive a power loss.

### Touch is inaccurate

Use the dashboard's **Calibrate touch** button and complete all four targets.
Calibration is stored in NVS until the flash is erased.

### COM port is busy

Close PlatformIO/Arduino serial monitors before erase or upload. Check the port:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device list
```

## 9. Factory workflow test

Erasing flash removes firmware, Wi-Fi, Dexcom credentials, settings, and touch
calibration. It does not erase the SD card.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t erase --upload-port COM12
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload --upload-port COM12
```

Use a blank SD card or temporarily remove it if testing a completely new system.

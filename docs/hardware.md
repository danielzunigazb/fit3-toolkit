# The Fit3's hardware, as the firmware names it

From the identifier-like strings of the main image and the functions that load them (`src/re/autoname.py`: 1,154 of about 11,000 functions named, 1,097 with one string; output in `local/re/autonames.csv`, derived from Samsung's firmware, not published). Not checked on the board: the names are what the code says, not a teardown.

| Part | What the firmware says |
|---|---|
| SoC | BES1502x family (Best Micro): `hal_i2c`, `hal_gpio`, `hal_uart` with DMA, `pmu_*` (VCORE, VANA, VCODEC), a 3.37 MB main image, a **vector GPU** (`vg_lite`, VeriSilicon) |
| Display | AMOLED over QSPI. Three driver ICs are built in and the right one is chosen by reading the panel ID: **ICNA3311**, **CH13613**, **ILI8688** (`lcd_*_readid`, `AT^LCDID?`). Brightness, ESD check, always-on mode (AOD), high-brightness mode |
| Touch | **STX** controller (`A8018` chip code); drivers for two other makers are present (`hyn_*`, `chsc`/`semi_touch`). Own firmware update (`tp_firmware_stx.bin`) |
| Charger | **SGM41562** (SG Micro), I2C |
| Battery gauge | **CW6305x** (CellWise), I2C |
| Ambient light | `als_drv_*` with a lux algorithm |
| Motion and health | a separate sensor-hub MCU, talked to over a UART with RPC (`MCU2SENS`, `sensor_hub_open`; its own firmware `sensor_hub.bin`, 714 KB). Optical heart-rate sensor (`PPG`), accelerometer, gyro; the AT list also names a barometer and a compass (not necessarily fitted) |
| Storage | NOR flash **GD25D20** (`_bp_init_gd25d20_40`, write-suspend) and NAND with a yaffs2 file system; PSRAM tests exist |
| Motor | `motor_timer_init`, 80 patterns (docs/details.md) |
| Radio | classic Bluetooth (`btm`, `btif`, `sdp`, `rfcomm`, `spp`) and BLE (RivieraWaves kernel: `ke`, `gapm`, `gapc`, `gatt`, `l2cap`, `hci`); radio calibration in `btdrv_*`. BLE services: the standard **Heart Rate** service (`hrps`), a Samsung one, and SmartThings find-device |
| Software | FreeRTOS, LVGL 8.3, ulog logging, the OOBE first-run flow (`oobe_*`, ship mode) |

Subsystems by named function (one-string names): `service` 232, `app` 196 (the pages), `l2cap` 47, `btm` 38, `bt` 37, `db` 31, `gatt` 30, `gapc` 26, `co` 21, `gapm` 20, `hci` 19, `tile` 18 (the widget tiles of the watch face editor).

## The factory commands (`AT^...`, 314 names)

Found as strings; only `00AT^OTA_UPDATE` is known to work from the serial service (`00` + the text). Four read-only queries sent the same way (`00AT^SWVER`, `00AT^LCDID?`, `00AT^GETBATPERCENT?`, `00AT^BTMAC?`) got **no answer**: they are probably on another input (a factory UART), not on this one. The families, to know what the firmware can do:

- **Test and calibration**: `FAC_*` (colour screens, charge on/off), `PPG_*` (optical sensor: open, current, gain, light leak, noise), `GET_*` (accelerometer calibration, barometer, light), `SR*` (sensor readings), `T*`/`TLE*`/`TBT*` (radio tests), `NAND_*`, `PSRAM_*`, `MOTOR_*`, `LIGHT_*`, `ACCEL_*`, `FT*` (production-line steps).
- **Information**: `SWVER`, `BTVERSION`, `LCDID?`, `TPINFO?`, `SOCID`, `BOARDID`, `BTMAC?`, `GETVBAT`, `GETBATPERCENT?`, `TBATTEMP?`, `GETSHELLTEMP?`, `ISCHARGE?`, `GETVBUS`, `DATETIME?`, `GETRTC`.
- **Data in** (as if from the phone): `WEATHER`, `CALENDAR`, `ALARM`, `TIMER`, `WORLD_CLOCK`, `MUSIC_CON`, `LANGUAGE`, `NOTICE`, `ALERT`, `HR`, `SPO2`, `SLEEP`, `DAILYACT`, `SPORT`, `HRSEND`, `PROTO`, and the watch faces (`WF_*`).
- **Raw access**: `MEMORY_READ`, `MEMORY_WRITE`, `IIC_READ_REG`, `IIC_WRITE_REG`, `SENSOR_DUMP_REG`, `SET_I2C`.
- **Dangerous: never send**: `FORMAT`, `FORMATFACT`, `SHIPMODE` (cuts the battery), `POWEROFF`, `REBOOT`, `MEMORY_WRITE`, `IIC_WRITE_REG`, `WATCHDOG_*`, `FAC_DISABLE_CHARGE`, `CHARGE_REG_SET`, `OTA_UPDATE`, and the `FI^` family (`FORMAT`, `RM`, `RMDIR`).

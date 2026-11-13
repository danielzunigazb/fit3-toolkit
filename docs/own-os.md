# Studying a replacement of Samsung's software (nothing here was flashed)

A study of the stock package and main image, on the PC only. Question: how much of the watch's software could be ours, and how safely.

## What the package updates, and what it does not

The package (73 components, 19.7 MB) holds the main image (`best1502x_b319_user.bin`, 3.37 MB), the sensor-hub firmware (714 KB), the touch-panel firmware (`tp_firmware_stx.bin`), `version.ini` and about 70 language packs. **The boot loader and the chip's ROM are not in it**: an update cannot damage them. Whether the boot loader falls back to a recovery mode when the main image is invalid is **not known**; BES chips usually wait for a UART or USB download in that case, which would need the watch opened.

The main image has a header at `0x1000` (`0x11223344`, `0xdeaddead`, a CRC32 and the size), is not encrypted and shows no signature (docs/firmware.md): images we changed have booted, four times on this watch. That is not a proof that nothing else checks it.

## How the system is built (from the image)

About 8,800 functions. FreeRTOS with a table of tasks (`gui_thread_def`, `0x2c2ef714`: name, stack and priority; the GUI task is `thread_gui`, stack 0x3000, entry `0x2c14c1b1`), among them `thread_gui`, `thread_sensorhub_comm`, `thread_bmt` (the Bluetooth side, with the file service we use), `thread_service`, `thread_resource` and the side-key thread. Everything visible goes through one window manager that lives in the GUI task: windows, transitions, key policy (docs/details.md). Fonts and images are on the flash file system, not in the image.

The hardware surface is large. The code loads about 350 distinct values that look like peripheral registers (an upper bound: some are constants); the display, the touch controller, the power and charging logic, the motor, the radio and the sensor link are all driven from the image. A Bluetooth radio driver (`bt_drv_reg_op_*`) is part of it. None of this has documentation.

## Three levels

1. **Our software on Samsung's system** (what the toolkit does): the loader sits in free flash and takes windows over. Low risk. Reaches everything the user sees.
2. **Our own GUI task on Samsung's drivers**: patch the entry of `thread_gui` in `gui_thread_def` to our own function, which does what the stock one does at start-up (`0x2c269e98`: LVGL, display, touch, fonts) and then runs **our** window system and event loop instead of Samsung's. Drivers, Bluetooth, the file system and the sensor link stay stock, called by address as the toolkit already does. This is "a system of our own" in every way a user can tell.
3. **A firmware from scratch**: needs the display and touch start-up, the power management (battery life depends on it), the Bluetooth radio and its host stack, and an update path. Years of work for one person with no documentation, and no way back by Bluetooth if it has a fault.

## The safety property that makes level 2 reasonable

The Bluetooth and file service run in their own task. If our GUI task is wrong, the watch shows a black or frozen screen, **but the radio keeps answering**, so the package can be flashed back as always. What would break that is a fault that resets the whole chip in a loop (a hard fault or the watchdog), because the radio would be up only for a few seconds each cycle.

A guard solves it: our entry starts by looking at a counter in `/user/` that it raises on every start and clears after the interface has worked for 20 seconds; if the counter says two starts did not reach that point, our entry calls the stock one instead, and the watch is Samsung's again.

## What level 2 has to keep from the stock system

- **The key dispatch**, `wm_key_dispatch` (`0x2c225f2c`): the long press of the side key is the power-off menu and `0x408` calls the **emergency SOS** (`0x2c15ee70`). Our loop must keep passing these keys to it and must never hook that function.
- **The queues** the services post to (notifications, charger, sensor hub, phone link): if nobody empties them the services may block. Our loop has to read them, even to ignore a message.
- **The power-state protocol** (`gui_power_state` `0x2010d0b0`, LCD on and off), or the screen never sleeps.

## A first experiment that cannot hurt

On the PC, in the emulator: run the stock start-up, then call our function instead of the stock GUI loop and see which queues it must read and what it must keep (a list of what the stock loop calls every pass). Nothing is flashed. Its result is the list above made precise, and a size for the work.

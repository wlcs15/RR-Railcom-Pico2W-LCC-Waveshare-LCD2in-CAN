# RR-Railcom Pico 2 W LCC — Waveshare 2 inch LCD and CAN

OwlThree node **05.01.01.01.A5.05**. This repository is the Pico 2 W LCC node: Wi-Fi GridConnect first, then a Waveshare Pico-LCD-2, then a Waveshare Pico-CAN-B on the same header. Application code is MIT, copyright Charles L. Sherman, 2026. See `NOTICE`.

Tag **v0.02** is the Pico 2 W Wi-Fi join. `lcc_node` reads the SSID and password from gitignored `local/wifi_psk_wrap.inc`, prints `TARGET ip`, and listens for GridConnect on TCP port 12021. The SSID is whatever you type into `scripts/provision_wifi.py`. It is not stored in source. No HAT is attached, and the three-pin SWD header is not soldered and not required.

Tag **v0.01** remains steps A through J: USB serial identity and the stacked pin map.

Host Unity tests run on the PC. `scripts/run_target_tests.sh` loads `lcc_node.uf2` and reads the USB self-check when the headered Pico 2 W is plugged in. That script cannot join Wi-Fi until `local/wifi_psk_wrap.inc` exists. The password is wrapped with this board's flash id and Wi-Fi MAC. Do not paste it into chat.

## What is next

1. **K and L are in v0.02.** `python3 scripts/provision_wifi.py` asks for the SSID and the password on a TTY, writes only ciphertext, and the Pico 2 W image joins that network and listens on port 12021.
2. **M and N are not the A5.01–A5.04 node yet.** The frames are a small login, not OpenMRN and not a full OpenLcbCLib CDI. JMRI Configure will not match those nodes until that stack is linked.
3. Do not flash the Mega or the RR-CirKits gateway.

Steps O through X wait on parts. O through R are the 2 inch display. S through X are Pico-CAN-B, including stacking headers and soldering the bare Pico 2 W. Y's pin table and the do-not-flash list are already in this file. Z stays in force: RailCom firmware and the RP2350-CAN board are not part of this repository.

## What to plug in

Use the **Waveshare Pico 2 W that already has headers**. Connect its **micro-USB** port to this PC. That is the only cable for these steps.

Do **not** connect the three-pin SWD debug header, and do not use a second USB Debug Probe. USB serial does not need it.

Leave the Pico 2 W **without headers** unsoldered. Long stacking pins are a later decision, after Pico-CAN-B is in hand (step X).

## Boards

| Role | Board | SDK name |
|---|---|---|
| Development MCU, this USB port | Pico 2 W with headers | `pico2_w` |
| No-Wi-Fi image check | Pico 2 with headers | `pico2` |
| RP2040 image check | Pico WH (wireless, headers) | `pico_w` |
| Display, on order | Pico-LCD-2, ASIN B0BD868MYK | not built yet |
| CAN, on order | Pico-CAN-B, ASIN B0BS9SDN4Q | not built yet |
| Do not solder yet | Pico 2 W without headers | same `pico2_w` image later |
| Drawer | 3.5 inch Pico-ResTouch, RP2350-CAN (B0F4JH65HY) | not this product |

`pico` remains a legal CMake board for a non-wireless Pico. It is **not** the image for the Pico WH. A Pico WH uses the wireless chip for its LED. GPIO 25 on that board is the wireless SPI chip-select, so a `pico` image must not be flashed onto it.

The 3.5 inch ResTouch uses GP5 and GP21, which Pico-CAN-B also uses. Do not stack those two. The RP2350-CAN board has no Wi-Fi, and its CAN controller uses GP8–GP12, which is the 2 inch display bus.

Do not flash the Mega (A5.02) or the RR-CirKits gateway.

## GPIO map

Both HATs can be attached at once. The display is SPI1. The CAN controller is SPI0. Wi-Fi uses GP23, GP24, GP25, and GP29, which are not on the user header. The onboard LED on `pico2_w` and `pico_w` is on the wireless chip, not GP25.

| Function | GPIO |
|---|---|
| LCD D/C, CS, clock, MOSI, reset, backlight | GP8, GP9, GP10, GP11, GP12, GP13 |
| KEY2, KEY3, KEY0, KEY1 | GP2, GP3, GP15, GP17 |
| CAN MISO, CS, clock, MOSI, interrupt | GP4, GP5, GP6, GP7, GP21 |
| Left unused until the CAN-B schematic is checked | GP19, GP22 |
| Left free | GP0, GP1, GP14, GP16, GP18, GP20, GP26, GP27, GP28 |

KEY0 stays on GP15 unless `-DRR_KEY0_GPIO` is set. Seat a HAT with its USB mark toward the Pico USB plug. Both HATs have female sockets, so one Pico with short male headers can hold only one of them until stacking pins are chosen.

## Build

Pico SDK 2.2.0 and ARM GNU 14.2. Host tests use LLVM Clang, not MinGW. From this directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_host.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_lizard.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_coverage.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_firmware.ps1
```

Ubuntu:

```bash
bash scripts/check_tools.sh
bash scripts/build_host.sh
bash scripts/run_tests.sh
bash scripts/run_lizard.sh
bash scripts/run_coverage.sh
bash scripts/build_firmware.sh
```

Firmware outputs:

| Board | UF2 |
|---|---|
| `pico2_w` | `build/firmware/pico2_w/lcc_node.uf2` |
| `pico2` | `build/firmware/pico2/lcc_node.uf2` |
| `pico_w` | `build/firmware/pico_w/lcc_node.uf2` |
| `pico` | `build/firmware/pico/lcc_node.uf2` |

Hold BOOTSEL, plug in the headered Pico 2 W, and copy `lcc_node.uf2` to the `RP2350` drive. Or, with picotool and the board already running this firmware:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\flash_lcc.ps1
```

USB serial is the Pico CDC port. The three-pin debug header is not used.

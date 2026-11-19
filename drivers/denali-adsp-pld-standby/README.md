# Denali ADSP PLD Modern Standby GLINK Client (`denali_adsp_pld_standby`)

Hardware-validated Linux RPMSG client for the Qualcomm Snapdragon X Elite (`X1E80100`) ADSP
Power Limit Driver (`PLD`) channel `APPS_ADSP_PWR_LMTS_GLINK_PORT` on Microsoft Surface Pro 11
(Denali OLED).

## Why This Exists

At ADSP boot, the Denali ADSP firmware starts a periodic `PLD` timer (~13.7 Hz) that reads SPMI ADC
channels and updates DDR shared memory. On Linux, `APPS_ADSP_PWR_LMTS_GLINK_PORT` is exported by
the ADSP with no driver bound, so the timer keeps firing during system suspend—pulling `DDR`,
`CXSD`, and `AOSD` out of low-power collapse >3,200 times in 4 minutes.

On Windows, `qcpep8380.sys` sends a 24-byte little-endian `MODERN_STANDBY_STATE` packet (`1` on
standby entry, `0` on exit) over `APPS_ADSP_PWR_LMTS_GLINK_PORT`, causing the ADSP firmware to
clear the `PLD` timer while suspended and restart it on wake.

## Measured Hardware Impact (Stage 4 & Stage 5)

With restricted `qcrypto` (clearing 58 startup interconnect floors), idle `lpass-wsa-macro` clock
release, and SMP2P `sleepstate` bit 12:

* **144.8 s deep-sleep capture (Stage 4):**
  * `CXSD`: **`99.962%`** (`2` entries vs `3,254` without PLD pause)
  * `AOSD`: **`99.961%`** (`2` entries vs `2,620` without)
  * `DDR`: **`99.964%`** (`2` entries vs `3,255` without)
  * `ADSP`: **`99.618%`** (`1.80 Hz` internal only, zero `DDR`/`CX`/`AOSS` wakeups)
* **80.8-minute battery-only deep sleep (Stage 5):**
  * `CXSD`/`AOSD`/`DDR`: **`99.911%`** (`17` entries total)
  * Measured battery drain: **`0.163 W`** (vs `0.1775 W` control without PLD pause)

## Files

* [`denali_adsp_pld_standby.c`](denali_adsp_pld_standby.c) — Out-of-tree RPMSG driver (default-off via `denali_pld_sleep_notify=N`; guards on `microsoft,denali-oled`, `6800000.remoteproc`, and `PM_SUSPEND_MEM`).
* [`Makefile`](Makefile) — Standard kbuild `obj-m` file.
* [`pld-callbacks-test.c`](pld-callbacks-test.c) — Standalone userspace unit test covering default-off, non-`PM_SUSPEND_MEM`, send errors, duplicate suspend rejection, and guaranteed exit restore.
* [`denali-adsp-standby.dtso`](denali-adsp-standby.dtso) — Companion Device Tree overlay wiring `smp2p-adsp/sleepstate` bit 12 to `&remoteproc_adsp`.

## Quick Verification (Userspace Unit Test)

```bash
gcc -Wall -Wextra -Werror pld-callbacks-test.c -o /tmp/pld-callbacks-test && /tmp/pld-callbacks-test
# Expected output: PLD_CALLBACKS_TEST_OK
```

## Out-of-Tree Kernel Build

```bash
make -C /path/to/kernel-build M=$PWD modules
```

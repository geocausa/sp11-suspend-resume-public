# 01 — Deep-Sleep SoC Residency (`CXSD`/`AOSD`/`DDR`) and ADSP PLD GLINK Pause

## Problem Summary

Earlier Linux trials on the Surface Pro 11 (`X1E80100`, Denali OLED) recorded **0% `CXSD` / `AOSD` collapse** and approximately **0.67–0.75 W** on battery. The later trial composition reached **99.96% SoC residency** in a short run and **0.163 W** in a separate long run. Those runs do not establish an unmodified unified13 battery baseline.

---

## Blocker 1 — 58 Unreleased Interconnect Startup Floors (`qcrypto` / QCE)

* **Cause:** Qualcomm's `qcrypto` driver was unbuilt/unbound (`CONFIG_CRYPTO_DEV_QCE`), so its interconnect providers never completed `sync_state`, leaving **58 startup interconnect floors** pinned active across suspend.
* **Fix:** [`patches/standby-power/01-candidate-qce-pm-opt-in.patch`](../patches/standby-power/01-candidate-qce-pm-opt-in.patch) (opt-in `do_register_algos=0`).
* **Result:** Binding restricted `qcrypto` clears all 58 startup floors (`0` remaining) and leaves QCE runtime-suspended (`usage=0`) with crypto algorithms disabled.

## Blocker 2 — Retained `lpass-wsa-macro` Hardware Clock Votes in System Sleep

* **Cause:** Even with all 19 BCM sleep buckets zeroed, the Denali WSA audio macro retained two Q6PRM hardware clock votes (`2/2` idle) across system suspend, preventing `CXSD` and `AOSD` collapse (`0%` residency even when `DDR` reached `94.8%`).
* **Fix:** [`patches/standby-power/02-candidate-denali-wsa-system-clock-release.patch`](../patches/standby-power/02-candidate-denali-wsa-system-clock-release.patch) (`denali_release_clocks_in_sleep=Y`).
* **Result:**
  * 21.5-minute battery run (`1,292.2 s`): `CXSD 93.17%`, `AOSD 91.64%`, `DDR 94.33%` (~`0.190 W`).
  * 2h 25m battery control (`8,721 s`, `37.100 → 36.670 Wh`): `CXSD 93.47%`, `AOSD 91.95%`, `DDR 94.58%` (**`0.1775 W`**).
  * **Remaining puzzle:** `ADSP` was still waking at **`13.73 Hz`** (`3,609` entries in `262.9 s`), pulling `DDR` (`3,255`), `CXSD` (`3,254`), and `AOSD` (`2,620`) out of collapse every ~73 ms.

## Blocker 3 — Decoded Windows ↔ ADSP `PLD` Modern Standby GLINK Protocol

Static disassembly of Windows `qcpep8380.sys` and Denali ADSP firmware (`qcadsp8380.mbn`) revealed why the ADSP woke at `13.73 Hz`:

1. At ADSP boot, `pld_init` initializes DDR shared-memory rings, spawns the ADSP Power Limit Driver (`PLD`) thread, and starts a periodic `PLD` timer. Every tick wakes the ADSP to read SPMI ADC channels (`U0V..U2I`, `B0V..B2ID`) over SPMI/AOSS and update DDR shared memory.
2. Linux exposes the RPMSG channel `6800000.remoteproc:glink-edge.APPS_ADSP_PWR_LMTS_GLINK_PORT.-1.-1` at boot with **no driver bound**.
3. On Windows Modern Standby entry (PEP state `0x7`) and exit (PEP state `0x8`), `qcpep8380.sys` sends a **24-byte (`0x18`) little-endian GLINK packet** over `APPS_ADSP_PWR_LMTS_GLINK_PORT`:

```c
struct pld_glink_ms_state_msg {
    __le32 size;                  /* 0x18 (24 bytes) */
    __le32 msg_type;              /* 0x01 (MODERN_STANDBY_STATE) */
    __le32 version;               /* 0x01 */
    __le32 modern_standby_state;  /* 1 = enter standby, 0 = exit standby */
    __le64 reserved;              /* 0 */
} __packed;
```

4. In ADSP firmware (`pld_gport_read_handler`):
   * `modern_standby_state = 1` clears the `PLD` timer (`timer_clr(&pld_timer, T_MSEC)`).
   * `modern_standby_state = 0` restarts the `PLD` timer.
5. In addition, Windows toggles outbound ADSP SMP2P `sleepstate` bit 12 (`0 → 0x1000 → 0`), implemented in [`patches/standby-power/03-candidate-denali-adsp-standby-notify.patch`](../patches/standby-power/03-candidate-denali-adsp-standby-notify.patch) + [`drivers/denali-adsp-pld-standby/denali-adsp-standby.dtso`](../drivers/denali-adsp-pld-standby/denali-adsp-standby.dtso). Note: SMP2P bit 12 alone did **not** stop the 13.73 Hz ADSP wakeups; the `PLD` GLINK message did.

## Hardware-Validated Results (Stages 4 & 5)

Using [`drivers/denali-adsp-pld-standby/denali_adsp_pld_standby.c`](../drivers/denali-adsp-pld-standby/denali_adsp_pld_standby.c) (`suspend_late` sends `1` before `sleepstate 0x1000`; `resume_early` sends `0` after `sleepstate 0x0`; ADSP returned `RX_DONE_W_REUSE` in ~1 ms):

* **Stage 4 (`144.775 s` battery deep sleep):**
  * `CXSD`: **`144.720 s` (`99.962%`, only `2` entries** vs `3,254` without PLD pause)
  * `AOSD`: **`144.718 s` (`99.961%`, only `2` entries** vs `2,620`)
  * `DDR`: **`144.722 s` (`99.964%`, only `2` entries** vs `3,255`)
  * `ADSP`: **`147.703 s` (`99.618%`, `1.80 Hz` internal only**, zero `DDR`/`CX`/`AOSS` wakeups)
  * `APSS`: `99.991%` (`1` entry); `CDSP`: `100.00%` (`0` entries)
  * `adsp_island`: remained `0`
* **Stage 5 (`4,846.265 s` / 80.8-minute battery deep sleep):**
  * `CXSD`/`AOSD`/`DDR`: **`99.911%`** (`17` entries total)
  * `BAT1`: `33.73 → 33.51 Wh` = **`0.1634 W`** (~14.1 mW saved vs the `0.1775 W` control)

## Remaining Gap (~58 mW vs Historical Windows ~0.105 W)

Stage 5's 0.163 W estimate has an approximate gauge-step range of **0.156–0.171 W**. The observed difference versus the 0.1775 W control was about 14 mW. The approximately 58 mW difference from historical Windows compares different sessions and conditions. Peripheral draw is one possible contributor. No matched energy test or direct rail measurement allocates the difference. [Shared-rail investigation](04-audio-ldo15b-and-soundwire.md) · [Windows reference and evidence](06-sp7-windows-evidence.md)

## Measurement Hygiene Note

`BAT1/energy_now` steps in `10 mWh` increments (`~5 mW` quantization per step over 2 hours, vs `~600 mW` over 1 minute). Any future standby-power comparison should use a **2-hour+ battery-only run** at midrange charge with matched peripherals, network state, and settled post-wake gauge samples.

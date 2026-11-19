# 02 — Wake Latency (`s2idle`), Touchscreen Async Resume PoC & SSAM Wake Gate

## Measured Kernel Resume Breakdown (`7.3.0-sp11-unified13`)

Three owner-attended `s2idle` (`freeze`) cycles on stock `7.3.0-sp11-unified13` (`A1`, `A2`, `A3`)
plus an earlier SSAM input-woken cycle (`Trial 02`) yielded consistent lossless ftrace timings
(`~3,255` paired PM callbacks, all returning status `0`):

| Metric / Callback | `A1` (Power Key) | `A2` (Power Key) | `A3` (Power Key) | `Trial 02` (SSAM Input) |
| :--- | ---: | ---: | ---: | ---: |
| **`dpm_resume_noirq` → `thaw_processes` end** | **2.562 s** | **2.541 s** | ~2.55 s | **2.482 s** |
| `mshw0485` touchscreen resume callback | 0.807 s | 0.809 s | 0.805 s | 0.812 s |
| `ath12k` Wi-Fi early resume (MHI wait) | 0.506 s | 0.510 s | — | 0.506 s |
| `msm` display driver completion phase | 0.459 s | 0.458 s | — | 0.452 s |
| Wi-Fi PCIe root (`1c08000.pcie`) noirq | 0.367 s | — | — | 0.360 s |
| Root NVMe PCIe (`1bf8000.pcie`) noirq | 0.315 s | — | — | 0.308 s |
| **NetworkManager Wi-Fi local activation** | **24.99 s** | **16.43 s** | **16.35 s** | — |

*(Note: Individual callbacks overlap across phases and must not be summed; owner-estimated
power-button-to-visible-screen on A2 was roughly 3–4 s.)*

---

## Touchscreen (`mshw0485_touch`) Resume Behavior & Default-Off Async PoC

### Why the callback takes 807–812 ms
In `g6ts_resume()`, `g6ts_power_on()` synchronously waits **500 ms** for rail power-up and **300 ms**
for reset, and then immediately schedules `recovery_work`, which performs its own full hardware
power-off / reset / power-on sequence (`100 ms` delay + `100 ms` off + `500 ms` + `300 ms` +
cold-attach waits `470 + 825 + 600 ms` ≈ `2.90 s`).

### Default-off prototype ([`patches/wake-resume/01-candidate-g6ts-async-resume-default-off.patch`](../patches/wake-resume/01-candidate-g6ts-async-resume-default-off.patch))
* Adds read-only module parameter `defer_resume_power_on` (default `false`).
* When opted in on paired-GPIO hardware (`ts->power_gpio && ts->reset_gpio`), sets `mode_enabled = false`
  and delegates initial power-on to `recovery_work` scheduled immediately (`0` jiffies), removing the
  redundant synchronous `800 ms` wait from `g6ts_resume()`.
* **Qualification status:** Passed 20/20 static contract checks, clean `checkpatch`, and full offline
  module compile/link against `7.3.0-sp11-unified13`. **NOT yet tested on hardware.**
* **Caution:** Kernel callback savings (~0.8 s) do not automatically equal perceived screen or
  first-usable-touch savings; measure both physically when testing.

### Intermittent post-wake panel reset (A2)
In `A2`, the touchscreen initialized cleanly at `+2.372 s`, received a single `RESET_RESPONSE`
notification from the panel at `+7.571 s`, completed a 3.060 s hardware recovery at `+10.631 s`,
and worked normally afterward. It did **not** recur in `A3`. Do not enable the legacy
`reset_recovery_v2` software path under `windows_init_parity=Y`—source inspection confirmed it
returns `-EOPNOTSUPP` and falls back to the same hardware reset anyway.

---

## PCIe / Wi-Fi / NVMe Resume Dependencies (Do Not Blindly Async)

* `ath12k` early resume (`~506 ms`) waits for MHI mission mode before `qrtr` early resume; moving
  `ath12k` to asynchronous background work without coordinating `qrtr` breaks an explicit dependency.
* `1bf8000.pcie` (`~315 ms`) serves the root NVMe filesystem.

---

## SSAM Keyboard/Touchpad Wake Architecture Gate

* **Hardware path verified:** Keyboard/touchpad-associated wake succeeded in `Trial 02` via
  `IRQ191` (`ssam_wakeup`), mapped cleanly as **`GPIO91 → PDC157 → GIC INTID862`** (matching
  Windows GPIO Bank 14). The earlier "wrong GPIO" hypothesis was disproved.
* **Why SSAM wake remains disabled by default:** Upstream disables wakeup on `serial1-0` because
  unfiltered EC notifications (battery/thermal/etc.) can cause spurious wakes.
* **Architecture gate for in-`s2idle` event filtering:**
  * `s2idle_ops` executes inside `kernel/power/suspend.c` between `dpm_suspend_noirq()` and
    `dpm_resume_noirq()`, while device IRQs are suspended, Qualcomm GENI UART (`b88000.serial`) is
    suspended via `pm_runtime_force_suspend()`, and SSAM is in `SSAM_CONTROLLER_SUSPENDED`
    (`ssam_request_sync_submit()` rejects requests with `-ENODEV`).
  * `s2idle_set_ops`, `pm_wakeup_clear`, and `rearm_wake_irq` are **not exported** in `Module.symvers`,
    and this platform boots Device Tree (no ACPI).
  * Therefore, selective SSAM wake filtering cannot be done in a loadable module or IRQ handler—it
    would require a built-in Qualcomm DT `s2idle_ops` implementation with safe partial UART/SSAM
    noirq wake and IRQ rearm logic. Parked as too invasive for current value.

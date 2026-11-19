# Surface Pro 11 (Snapdragon X Elite / Denali OLED) — Linux Suspend, Resume & Power Archive

This repository is a source and research archive of Linux `v7.3` kernel patches, standby-power
drivers, wake-latency measurements, and reverse-engineering findings for the **Microsoft Surface Pro 11
(OLED, Qualcomm Snapdragon X Elite `X1E80100`, codename Denali)**.

> **Project status (19 November 2026).** Unified13 is the working Linux baseline on the tested unit.
> The lowest standby draw came from a separate trial. Windows testing ran on the same SP11,
> with the SP7 as the analysis workstation and evidence store.

The [independent verification](docs/08-evidence-verification.md) reproduces the source and recalculates the measurements. It marks the open questions on everyday deep wake, unmodified unified13 standby draw and the Windows energy difference.

This release contains the kernel patch series, experimental source, configuration and reviewed
findings. The installation also needs firmware, audio topology and calibration specific to
the device. See the [source replay and installation notes](docs/07-reproduce-and-qualify.md)
for the required inputs and each trial's validation status.

---

## Highlights — What Was Proven on Hardware

### 1. Deep-Sleep SoC Residency: 0% → 99.96% (`CXSD` / `AOSD` / `DDR`) and 0.163 W Battery Standby

| Stage | Configuration | `CXSD` | `AOSD` | `DDR` | ADSP Wake Rate | Battery Drain |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Earlier Linux power baseline** | Historical trial, QCE unbound and WSA clocks retained | `0%` | `0%` | — | continuous | ~`0.67–0.75 W` |
| **+ QCE + WSA clock release** | Restricted `qcrypto` (`do_register_algos=0`, clears 58 startup ICC floors) + idle `lpass-wsa-macro` clock release | `93.47%` | `91.95%` | `94.58%` | `13.73 Hz` (`3,609` wakes / 263 s) | `0.178 W` (2h 25m run) |
| **+ ADSP PLD GLINK pause** | Trial adds PLD standby signaling plus SMP2P `sleepstate` bit 12 | **`99.96%`** (145 s run) | **`99.96%`** | **`99.96%`** | `1.80 Hz` (internal entries in short run) | **`0.163 W`** (separate 80.8 min run, `99.91%` residency) |
| *Historical Windows reference* | *Windows 11 Modern Standby (separate historical run)* | *99.18% HW DRIPS* | — | — | — | *~`0.105 W`* |

See [`docs/01-standby-power-cxsd-aosd-pld.md`](docs/01-standby-power-cxsd-aosd-pld.md) and
[`drivers/denali-adsp-pld-standby/`](drivers/denali-adsp-pld-standby/).

### 2. Wake-Latency Breakdown (`s2idle` on `7.3.0-sp11-unified13`)

Three owner-attended `s2idle` cycles (`A1`, `A2`, `A3`) with lossless ftrace (`~3,255` paired PM callbacks):
* **Kernel resume (`dpm_resume_noirq` → userspace thaw):** **`2.54–2.56 s`**
  * `mshw0485` touchscreen callback: `0.81 s` (500 ms power + 300 ms reset synchronous wait)
  * `ath12k` Wi-Fi early resume (MHI wait): `0.51 s`
  * `msm` display driver completion: `0.46 s`
  * Wi-Fi PCIe root (`1c08000.pcie`) noirq: `0.37 s`
  * Root NVMe PCIe (`1bf8000.pcie`) noirq: `0.31 s`
* **Wi-Fi reconnect after wake (`~16.3 s`):** A3 measured **15.60 s** in the first scan, whose request listed 101 frequencies. The driver review found an omitted priority field and a resume path that skips cached country reprogramming. Their effect on latency remains untested.

See [`docs/02-wake-latency-and-g6ts-touch.md`](docs/02-wake-latency-and-g6ts-touch.md) and
[`docs/03-wifi-ath12k-post-wake-scan.md`](docs/03-wifi-ath12k-post-wake-scan.md).

---

## Repository Map

* [`STATUS.md`](STATUS.md) — Concise status matrix (Hardware-Validated / Prototype / RFC / Open / Falsified).
* [`docs/07-reproduce-and-qualify.md`](docs/07-reproduce-and-qualify.md) — Replay the kernel source, identify the additional install inputs, and select the correct trial base.
* [`configs/config-7.3.0-sp11-unified13`](configs/config-7.3.0-sp11-unified13) — Accepted Linux `7.3` kernel `.config`.
* [`drivers/denali-adsp-pld-standby/`](drivers/denali-adsp-pld-standby/) — Hardware-validated ADSP PLD Modern Standby GLINK client driver (`denali_adsp_pld_standby.c`), offline unit test, and DT overlay.
* [`patches/`](patches/) — Organized patch sets with exact base commits and apply instructions:
  * [`patches/unified13-base/`](patches/unified13-base/) — Complete 47-patch SP11 Linux `v7.3` baseline (touchscreen QSPI/GPI, SoundWire/AudioReach protected speakers, ath12k rfkill, DTS, AppArmor).
  * [`patches/standby-power/`](patches/standby-power/) — QCE floor release, WSA884x sleep clock release, ADSP SMP2P sleepstate notify, PCIe PM observer.
  * [`patches/wake-resume/`](patches/wake-resume/) — Default-off G6TS async resume PoC + `ath12k` post-wake scan RFC patch.
* [`docs/`](docs/):
  1. [`01-standby-power-cxsd-aosd-pld.md`](docs/01-standby-power-cxsd-aosd-pld.md) — Complete deep-sleep power progression & decoded ADSP PLD GLINK protocol.
  2. [`02-wake-latency-and-g6ts-touch.md`](docs/02-wake-latency-and-g6ts-touch.md) — `s2idle` wake timings, touchscreen async prototype, and SSAM wake architecture gate.
  3. [`03-wifi-ath12k-post-wake-scan.md`](docs/03-wifi-ath12k-post-wake-scan.md) — Measured post-wake scan and untested source hypotheses.
  4. [`04-audio-ldo15b-and-soundwire.md`](docs/04-audio-ldo15b-and-soundwire.md) — SoundWire resume behavior & shared `LDO15_B` Windows/Linux evidence.
  5. [`05-falsified-and-parked-leads.md`](docs/05-falsified-and-parked-leads.md) — Disproved hypotheses & dead-ends to avoid repeating.
  6. [`06-sp7-windows-evidence.md`](docs/06-sp7-windows-evidence.md) — SP7 research index, Windows measurements, source provenance and evidence limits.
  7. [`08-evidence-verification.md`](docs/08-evidence-verification.md) — Rechecked measurements, source reproduction and recovered Wi-Fi capture boundary.

## Safety & Attribution Notes

* The later Linux trials used a separate boot entry or default-off experimental hooks, with accepted unified13 available as the fallback.
* Kernel patches in `patches/unified13-base/` include upstream and community backports on
  Linux `v7.3` (`c03c1a14507d`). They preserve upstream and community credits and links.
  Project-authored patches use a GitHub no-reply address.
* Windows protocol findings (ADSP `APPS_ADSP_PWR_LMTS_GLINK_PORT` packet format, SMP2P `sleepstate`
  bit 12, GPIO Bank 14 mapping, AUCD/UCS0 resource clauses) were derived from local clean-room
  static/ETW analysis; no proprietary binaries or firmware files are redistributed here.

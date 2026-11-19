# Technical Status Matrix (2026-11-19)

Target hardware: **Microsoft Surface Pro 11 OLED (`microsoft,denali-oled`, Qualcomm `X1E80100`)**
Base kernel: **Upstream Linux `v7.3` (`c03c1a14507d`)** + 47 patches (`7.3.0-sp11-unified13`)

| Category | Component / Workstream | Status | Summary |
| :--- | :--- | :--- | :--- |
| **Daily Driver** | `7.3.0-sp11-unified13` base kernel (`47` patches) | **HARDWARE-VALIDATED** | Boots cleanly; OLED, NVMe, USB-C, battery (`BAT1`), SSAM keyboard/trackpad, MSHW0485 touch, Wi-Fi 7 (`ath12k`), SoundWire + WSA884x protected audio, and short `s2idle` cycles work. |
| **Standby Power** | `qcrypto` restricted bind (`do_register_algos=0`) | **HARDWARE-VALIDATED** | Releases all 58 startup interconnect floors without enabling QCE crypto algos. |
| **Standby Power** | `lpass-wsa-macro` sleep clock release | **HARDWARE-VALIDATED** | Unlocks `CXSD` (93.5%), `AOSD` (92.0%), `DDR` (94.6%) in deep sleep (~0.178 W over 2h 25m). |
| **Standby Power** | `qcom_q6v5_pas` SMP2P `sleepstate` bit 12 | **HARDWARE-VALIDATED** | Verified `0 → 0x1000 → 0` outbound signaling in late suspend / early resume. |
| **Standby Power** | `denali_adsp_pld_standby` GLINK client | **HARDWARE-VALIDATED TRIAL** | The composed trial reached **99.96%** residency in 145 s and **0.163 W** with **99.91%** residency in a separate 80.8 min run. |
| **Standby Power** | Remaining ~58 mW difference vs historical Windows (~0.105 W) | **OPEN** | A comparison across separate runs. The source of the remaining draw is unmeasured, including any contribution from shared `LDO15_B`. |
| **Wake Latency** | Kernel `s2idle` resume (`2.54–2.56 s`) | **MEASURED** | Dominated by `mshw0485` (0.81 s), `ath12k` MHI (0.51 s), `msm` display (0.46 s), Wi-Fi PCIe (0.37 s), NVMe PCIe (0.31 s). |
| **Wake Latency** | `mshw0485_touch` `defer_resume_power_on` PoC | **COMPILED / UNTESTED** | Clean 38-line default-off patch; passed static contracts & offline module link. Hardware first-touch latency not yet measured. |
| **Wi-Fi Wake** | ~16 s post-wake reconnect (`15.60 s` first scan) | **MEASURED / RFC UNTESTED** | The scan accounts for almost all of the observed wait. The RFC covers two driver findings. Firmware scheduling and the improvement from either change remain unmeasured. |
| **Audio** | SoundWire post-deep-sleep speaker detach / Alert | **INTERMITTENT / OPEN** | Controller runtime resume and an audio graph restart helped in some trials. At least one trial needed a return to unified13. |
| **Touchscreen** | Post-wake panel reset notification (`+7.57 s` in A2) | **INTERMITTENT / OPEN** | Observed once in A2 (triggered 3.06 s hardware recovery); did not recur in A3. |
| **Input Wake** | SSAM keyboard/touchpad selective wake | **PARKED (ARCH GATE)** | Physical wake via GPIO91 → PDC157 → GIC862 works, but filtering spurious EC events in `s2idle` requires a built-in Qualcomm DT `s2idle_ops` partial-UART resume path. |
| **Windows** | SP7 cross-OS evidence | **REVIEWED** | SP7 replays confirm USB-C software votes and preserve negative AUCD observations. Physical rail state remains open. [Evidence index](docs/06-sp7-windows-evidence.md) |
| **Disproved** | `ath12k` WoWLAN/M3 alignment | **FALSIFIED ON TESTED PATH** | The proposed hook was unreachable with WoWLAN disabled. The driver already executed its WCN7850 power-off sequence. |

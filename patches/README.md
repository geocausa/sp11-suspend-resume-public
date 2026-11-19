# Patch Series Index

All patches in this directory are based on upstream **Linux `v7.3` (`c03c1a14507d`)** unless noted.

## 1. `unified13-base/` (47 patches — Hardware-Accepted Daily Driver)

Apply in order (`series`) on top of upstream Linux `v7.3` (`c03c1a14507d`) to reproduce the public unified13 source tree. The project uses a GitHub no-reply address for the two local maintainer contacts. See [source replay and installation inputs](../docs/07-reproduce-and-qualify.md).

* `0001–0005`, `0032–0033`, `0035–0038`: QSPI GPI DMA transport + Surface Pro 11 `mshw0485` touchscreen driver & DTS.
* `0006`, `0039`: `ath12k` rfkill skip on Surface + vdev restart count fix.
* `0007–0021`, `0025–0031`, `0034`, `0043–0046`: SoundWire protected feedback, AudioReach `q6apm`, `lpass-wsa-macro`, `lpass-va-macro`, and `wsa884x` Denali speaker/mic parity.
* `0022–0024`: Denali DTS volume keys, DisplayPort audio, and QREF regulator supplies.
* `0040–0041`: Ubuntu 7.3 AppArmor notification/mediation support (needed for Ubuntu snap compatibility).
* `0042`: Surface HID empty descriptor guard.
* `0047`: `driver core` sync-state link skip diagnostic fix.

## 2. `standby-power/` (Hardware-Validated Trial Patches on Top of `unified13`)

| File | Qualification | Target Base | Notes |
| :--- | :--- | :--- | :--- |
| `01-candidate-qce-pm-opt-in.patch` | **Hardware-validated** | `unified13` (`git apply --check` clean) | Backport of opt-in `CRYPTO_DEV_QCE` (`do_register_algos=0`). Releases all 58 startup interconnect floors. |
| `02-candidate-denali-wsa-system-clock-release.patch` | **Hardware-validated** | `unified13` (`git apply --check` clean) | Opt-in (`denali_release_clocks_in_sleep=Y`) release of idle Denali WSA hardware clock votes in system suspend; unlocks >93% `CXSD`/`AOSD`/`DDR`. |
| `03-candidate-denali-adsp-standby-notify.patch` | **Hardware-validated on the trial** | `unified15` USB trial `f27d6713fbbf3762a061d830dc6cd0ff4eb80558` | Adds opt-in outbound SMP2P `sleepstate` bit 12. The full patch's DTS hunk fails on unified13. The supplied overlay merges into the unified13 DTB offline. A matching PAS build and hardware trial are separate. |
| `04-candidate-denali-pcie-pm-observe.patch` | **Hardware-validated (diagnostic)** | `unified13` (`git apply --check` clean) | Opt-in (`denali_pm_observe=Y`) PARF PM status & L2 counter sampler around `PME_Turn_Off`. |

## 3. `wake-resume/` (Prototypes & RFCs on Top of `unified13`)

| File | Qualification | Target Base | Notes |
| :--- | :--- | :--- | :--- |
| `01-candidate-g6ts-async-resume-default-off.patch` | **Compiled & linked offline; NOT hardware-tested** | `unified13` (`git apply --check` clean) | Default-off (`defer_resume_power_on=Y`) moves synchronous 800 ms GPIO power-on out of `g6ts_resume()` to existing `recovery_work`. |
| `02-rfc-ath12k-scan-priority-and-resume-regdom.patch` | **Source-verified RFC; NOT compiled or hardware-tested** | `unified13` (`git apply --check` clean) | Writes missing `cmd->scan_priority` in `ath12k/wmi.c:2704` and restores cached `ar->alpha2` on resume in `ath12k/mac.c:13549`. |

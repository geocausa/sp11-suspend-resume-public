# 05 — Falsified Hypotheses & Parked Leads (Save Yourself the Detour)

These observations apply to the tested Surface Pro 11 OLED configuration. Parked paths retain open questions.

---

### 1. `ath12k` WoWLAN / M3 suspend alignment — **FALSIFIED**
* **Claim:** `1c08000.pcie: Timeout waiting for L2 entry! LTSSM: 0x11` means Wi-Fi/WCN7850 stays
  powered during suspend, and setting `disable_wow_m3=1` in `ath12k_wow_op_suspend()` fixes standby drain.
* **Verified reality:** With WoWLAN disabled (`iw phy phy0 wowlan show` → disabled), `mac80211`
  never calls `ath12k_wow_op_suspend()` at all—the patch is **dead code**. `ath12k_core_suspend_late()`
  already runs `ath12k_pci_power_down(ab, true)` → `ath12k_pci_sw_reset(ab, false)`, which holds the
  endpoint in reset before `dw_pcie_suspend_noirq()` sends `PME_Turn_Off`. The observed L2 poll warning
  is consistent with that ordering. `qcom_pcie_host_deinit()` then asserts `PERST#`
  and calls `pci_pwrctrl_power_off_devices()` (`WLAN_EN=0`, `wcn7850-pmu` regulators & clocks `1 → 0`).
  This shows the software power-off sequence. It does not measure electrical state on shared rails.

### 2. SSAM "Wrong GPIO Mapping" Hypothesis — **FALSIFIED**
* **Claim:** Keyboard/touchpad wake failed because Linux mapped the wrong GPIO/IRQ for SSAM.
* **Verified reality:** `GPIO91 → PDC157 → GIC INTID862` (`IRQ191` `ssam_wakeup`) matches decoded
  Windows GPIO Bank 14 and successfully woke the machine from `s2idle` in attended `Trial 02`.
  No IRQ-routing patch is needed; upstream keeps it disabled purely due to unfiltered EC wakes.

### 3. Wi-Fi Post-Wake 16 s Delay Caused by DFS Channels or NetworkManager 15 s Timeout — **FALSIFIED**
* **Verified reality:** Across 59 historical wakes, the same ~16.3 s delay occurred identically on
  non-DFS channel 44 (`5220 MHz`) and on normal desktop wakes where NetworkManager logged zero link
  timeouts. Neither a DFS channel nor that timeout is necessary for the delay to occur. A3 measured
  `15.60 s` waiting for the first scan. The driver hypotheses are described in the [scan review](03-wifi-ath12k-post-wake-scan.md).

### 4. Fixing `ath12k` Post-Wake `WORLD` Regdom with `iw reg set` or `cfg80211.ieee80211_regdom` — **INEFFECTIVE ON `unified13`**
* **Verified reality:** `CONFIG_ATH_REG_DYNAMIC_USER_REG_HINTS` is not set in `config-7.3.0-sp11-unified13`,
  so `ath12k_reg_notifier()` (`ath12k/reg.c:89-93`) immediately returns and ignores user regulatory
  hints. Either fix `ath12k` in-driver or rebuild with that Kconfig option if testing user hints.

### 5. Enabling Legacy `mshw0485` `reset_recovery_v2` Software Recovery — **INEFFECTIVE UNDER PARITY INIT**
* **Verified reality:** Under `windows_init_parity=Y`, the legacy software re-enumeration branch
  returns `-EOPNOTSUPP` after descriptor exchange and falls back to the same hardware reset anyway,
  adding extra delay rather than removing it.

### 6. Custom Built-in Qualcomm DT `s2idle_ops` SSAM Early-Wake Filter — **PARKED**
* **Verified reality:** Cannot be built as a loadable module (`s2idle_set_ops`, `pm_wakeup_clear`,
  `rearm_wake_irq` are unexported) and requires partially resuming Qualcomm GENI UART + SSAM inside
  the `noirq` suspended window. Too invasive for the UX benefit while power-key wake works reliably.

### 7. Repeating Quiet Awake / AC Windows ETW Captures for `LDO15_B` — **PARKED**
* **Verified reality:** Multiple zero-loss awake and AC Modern Standby ETW captures showed zero
  AUCD component 2/4 transitions (`CODEC_PSTATE_SET` stayed 0) and cached `UCS0` AC votes. Do not
  repeat the same capture without a new trigger or a battery-only Linux drain test justifying it.

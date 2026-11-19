# 03 — Wi-Fi (`ath12k` / WCN7850) post-wake scan timing

## Observed Problem

Of 61 recorded sleep exits from October 28 through November 18, 59 showed authentication or activation within 90 seconds. Among the 55 with a recorded activation, the medians were **16.378 s** with a NetworkManager wake notice (35 activations) and **16.267 s** without one (20 activations). Two earlier non-DFS wakes activated at 16.28 and 16.31 seconds. This observational selection describes reconnect timings. It does not establish outage rates or a controlled effect of any one variable.

Attended trace **`A3`** (with `cfg80211:rdev_scan`, `mac80211:drv_hw_scan`, `cfg80211:cfg80211_scan_done`,
and `iw event -t`) isolated the bottleneck:

| Event | Time relative to `PM: suspend exit` |
| :--- | ---: |
| Processes finished thawing | −0.124 s |
| `wpa_supplicant` requested scan → `drv_hw_scan` | −0.024 s |
| `iw` reported scan started | +0.033 s |
| Driver regulatory domain switched `WORLD` → `GB` | **+7.021 s** |
| First scan completed (**101 frequencies**: 2.4 / 5 / 6 GHz) | **+15.576 s** |
| Authentication started | +15.648 s (+72 ms) |
| Local Wi-Fi activated (DHCP complete) | +16.345 s (+769 ms) |

**The first scan occupied `15.600 s` (~95% of total reconnect time).** The requested frequency list does not establish the firmware's actual visit order or dwell time. Historical logs show a similar cadence on other wakes and boots.

## Awake Control Measurement (2026-11-19)

Running one non-disruptive `nmcli dev wifi rescan` while awake, connected, and already in `GB` (no
11d scan running) measured:

| Condition | Regdom | 11d status in review | Requested frequencies | Scan duration |
| :--- | :--- | :--- | :--- | ---: |
| Post-wake A3 (disconnected) | `WORLD` (`00` → `GB` at `+7.02 s`) | Yes | **101** (`13` 2.4G, `28` 5G, `60` 6G) | **15.600 s** |
| Awake connected control | `GB` (`DFS-ETSI`) | No | **64** (`13` 2.4G, `27` 5G, `24` 6G) | **9.406 s** |

---

## Driver findings and hypotheses

### 1. The accepted source omits `cmd->scan_priority` (`ath12k/wmi.c:2700-2707`)
In `ath12k_wmi_send_scan_start_cmd()`:
```c
cmd->scan_id = cpu_to_le32(arg->scan_id);
cmd->scan_req_id = cpu_to_le32(arg->scan_req_id);
cmd->vdev_id = cpu_to_le32(arg->vdev_id);
if (ar->state_11d == ATH12K_11D_PREPARING)
    arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
else
    arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
cmd->notify_scan_events = cpu_to_le32(arg->notify_scan_events);
```
`struct wmi_start_scan_cmd` has `__le32 scan_priority` directly after `vdev_id`, but **`cmd->scan_priority = cpu_to_le32(arg->scan_priority)` is missing**. Because the SKB is zero-initialized, firmware always receives `WMI_SCAN_PRIORITY_VERY_LOW` (`0`).

The source can also launch firmware 11d country discovery in this path. A3 did not record firmware scheduling or establish that 11d preempted the host scan. The interval before GB returned is a correlation.

### 2. The radio returned through `WORLD` in A3 (`reg.c:919`, `mac.c:13549`)
* Without WoWLAN, WCN7850 cold-boots on resume and sends a default `"00"` (`WORLD`) regulatory event.
* `ath12k_reg_validate_reg_info()` (`reg.c:919-922`) only drops duplicate default `"00"` events when
  `!ab->new_regd[pdev_idx]`. Once `"GB"` was learned earlier in the boot, `ab->new_regd[0]` is
  non-NULL, so an eligible `"00"` event can reset `phy#0` back to `WORLD`.
* `ath12k_mac_op_reconfig_complete()` (`mac.c:13537-13573`) already restores cached `ar->alpha2`
  after crash recovery (`IEEE80211_RECONFIG_TYPE_RESTART`), but returns early on line 13549 for
  suspend/resume (`IEEE80211_RECONFIG_TYPE_SUSPEND`).
* `ath12k_mac_vdev_create()` (`mac.c:10608-10613`) can re-arm `ar->state_11d = ATH12K_11D_PREPARING`.
* Because `CONFIG_ATH_REG_DYNAMIC_USER_REG_HINTS` is disabled in `config-7.3.0-sp11-unified13`,
  `ath12k_reg_notifier()` (`reg.c:89-93`) ignores userspace `iw reg set` hints.

### 3. Broad scan scope and dwell settings
* `ath12k_wmi_start_scan_init()` (`wmi.c:2541-2547`) sets: `dwell_time_active=50 ms`,
  `dwell_time_passive=150 ms`, `dwell_time_active_6g=70 ms`, `dwell_time_passive_6g=70 ms`,
  `min_rest_time=50 ms`, `max_rest_time=500 ms`.
* Connected and post-wake scans differ in link state, regulatory domain and requested channels. The 9.406 s awake scan is a useful point of comparison. Subtracting it from A3 cannot isolate 11d time, per-channel dwell or home-channel rest overhead.
* `ath12k_regd_update_chan_list_work()` (`reg.c:835-847`) can wait for the active scan before updating firmware's channel list. A3 did not independently trace that callback.

## Status of the proposed changes

The [ath12k RFC](../patches/wake-resume/02-rfc-ath12k-scan-priority-and-resume-regdom.patch) writes the missing priority field and extends cached country reprogramming to suspend reconfiguration. Only patch applicability was checked. Future work would need to qualify the changes independently, including regulatory behavior on a moved device. Known-frequency-first userspace scanning with a full-scan fallback is also a candidate. No backend switch or post-patch latency was measured.

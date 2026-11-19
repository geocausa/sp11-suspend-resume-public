# 04 — Audio (`WSA884x` / SoundWire) and Shared `LDO15_B` Rail Findings

## Working Audio Baseline (`7.3.0-sp11-unified13`)

`unified13` integrates protected AudioReach/SoundWire playback for the dual Denali `WSA884x`
smart amplifiers (`patches/unified13-base/`). Cold playback and post-`s2idle` playback (`A3`) were
confirmed clear on both speakers (with PipeWire/UbiG userspace protection graph).

## Open Audio Reliability Issue: Intermittent SoundWire Detach / Alert After Deep Sleep

* On deep-sleep trials (Nov 14–15), both `WSA884x` amplifiers occasionally remained `Detached` or
  logged SoundWire bus-clash/parity (`Alert` status) on the first post-wake playback.
* Controller runtime resume and a PipeWire/WirePlumber restart recovered sound in some trials.
  In a DSP trial where the amplifiers stayed detached, returning to unified13 restored both speakers.
* Startup/post-resume logs also show non-fatal `qcom-apm DSP Error (3) Processing 0x01001006`
  (`AR_EUNSUPPORTED`); this message alone does not prove audible failure (A3 produced clear sound).

---

## Shared Rail Investigation (`LDO12_B` and `LDO15_B`) — Current Evidence & Boundaries

After Stage 5 reached **99.91% `CXSD`/`AOSD`/`DDR` residency** at `0.163 W`, board-level rails
were audited to investigate the remaining ~58 mW gap vs historical Windows (~0.105 W).

### Live Linux Denali Topology (`7.3.0-sp11-unified13`)
* **`LDO15_B`** (no `regulator-always-on`): 3 active consumer references while awake/runtime-suspended —
  both `WSA884x` speaker codecs + `WCN` (Wi-Fi/BT).
* **`LDO12_B`** (no `regulator-always-on`): 2 active consumer references — both `WSA884x` speaker codecs.
* Both `WSA884x` codecs enable their supplies at probe and retain references through runtime and
  system suspend; neither consumer nor `WCN` declares load currents (`uA`), and live Denali
  `ldo12`/`ldo15` lack load-control permissions/allowed modes. RPMh regulators also do not implement
  `set_suspend_mode`, so a DTS-only mode change is insufficient.

### Clean-Room Windows Static & ETW Findings (Nov 15–18)
* **USB-C (`UCS0` component 6):** Confirmed bidirectional software requester on `LDO15_B` via
  attended awake charger insertion (`[1800000 uV, enable=1, mode=7]`) and removal (`[0, 0, 4]`)
  ETW PPP captures. In an AC Modern Standby trace (38.1 s sleep, 15.3 s HW DRIPS), `UCS0`
  component 6 stayed active (consistent with a cached AC vote).
* **Audio (`qcaucd8380` components 2 and 4):** Static resource programs for components 2 and 4
  include `PSTATE 1` clauses requesting lower mode `4` / `enable=0` on `LDO15_B`, mapped from
  payload fields `+0x0c` and `+0x04` in selector-1 of the registered resource callback.
* **What was NOT observed:** Across multiple zero-loss awake and AC Modern Standby ETW captures
  (including `qcpep` owner message `0x5d`, PPP, RuntimeFx, and `qcaucd` WPP), AUCD components 2
  and 4 stayed in `CODEC_PSTATE_SET` index `0` with **zero observed transitions**, and no electrical
  rail measurement or battery-mode `LDO15_B` shutdown was captured.

### Guidance for Future Contributors
* **Do not blindly disable or force LPM on `LDO15_B`/`LDO12_B`:**
  1. `LDO15_B` is shared with `WCN`; undercounting shared current demand can brown out Wi-Fi/BT.
  2. Power-gating the `WSA884x` amplifiers requires verifying full codec context restore on wake
     (distinct from ordinary SoundWire clock-stop detach).
  3. No measurement has yet established how many milliwatts (if any) `LDO15_B` actually contributes
     to the ~58 mW board standby delta.
* **Recommended order:** First take a clean 2-hour battery-only standby baseline on `unified13`
  (and/or with the PLD pause) before investing further effort in Windows `LDO15_B` reverse engineering.

[SP7 archive index and per-capture limits](06-sp7-windows-evidence.md)

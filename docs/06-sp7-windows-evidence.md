# Windows standby evidence analyzed on the Surface Pro 7

The **Surface Pro 11 OLED (Snapdragon X Elite / Denali)** was the device under test. The **Surface Pro 7 (Intel)** was the Windows analysis workstation and evidence store. It replayed SP11 ETLs and hosted static analysis. Its own standby behavior is outside this comparison.

## What the Windows work established

| Evidence | Observation | Boundary |
| --- | --- | --- |
| October 27 retained SP11 driver images | The RPE receiver copied raw states `4` and `6` into a queued work item. The PMIC client used matching GLINK link notifications to clear and rearm local state on link-down and call the channel opener on link-up. | The producer and formal meaning of raw `4` remained open. A local open request, channel acknowledgement and completed reinitialization are separate milestones. The analysis verified 20 functions and 2,632 ARM64 instructions. |
| Historical Windows battery Modern Standby | About **0.105 W** and **99.18% hardware DRIPS** in the earlier DC reference used for the Linux comparison. | Separate runs and conditions. The November short ETLs below do not provide new standby watts. |
| November 16, same SP11 Windows boot, AC then battery | The approximately **78 s AC** and **166 s DC** Modern Standby captures returned normally. AUCD components 2 and 4 stayed in codec performance state `0`, with no observed transitions and no resource 20 request observed. | The short battery run did not resolve an energy delta. Both captures began after the charger condition was established. Its earlier cable transition was outside the interval. |
| November 16, two awake charger transitions | The same USB-C client, UCS0 component 6, issued an `LDO15_B` request on connection and released its request on removal. The per-client operands were `[1800000 µV, enable=1, mode=7]` and `[0 µV, enable=0, mode=4]`. Independent SP7 replays matched the 324 insertion and 222 removal PPP records. | These are pre-aggregation software requests. Effective regulator voltage, current and other clients' votes were outside the capture. |
| November 18, AC Modern Standby | **38.132 s** of S0 standby, **15.297 s** of hardware DRIPS, and no lost trace events or buffers. **108** validated owner requests and **292** PPP resource-detail events contained no resource 20. AUCD 2/4 remained at state `0`, and UCS0 component 6 remained active. | Earlier cached votes could persist. The absence of a request in this interval leaves effective rail state unknown. |
| November 18, 90 s targeted awake audio capture | Zero-loss Owner, PPP and RuntimeFx traces produced **120** validated owner requests and **344** PPP details, none for resource 20. AUCD 2/4 had no performance-state transition and the dedicated AUCD WPP stream produced zero matching messages. | Playback timing was unverified and no positive audio-provider marker appeared. This run neither demonstrated nor excluded the target audio release path. |

## How the pieces connect to Linux

- **Power residency.** The separately decoded ADSP PLD standby notification informed the Linux experiment. Stage 4 reached approximately 99.96% low-power residency over 145 seconds. Stage 5 measured 0.163 W over 80.8 minutes on the trial configuration. [Linux trial and conditions](01-standby-power-cxsd-aosd-pld.md)
- **Shared regulator ownership.** Static Windows resource tables identify release clauses for AUCD components 2 and 4, while traces prove a USB-C client's charger-dependent vote. Linux has WSA884x and WCN references to the same supply. This evidence establishes possible consumers and observed requests. It leaves the aggregate, electrical state and contribution to standby draw unresolved. [Audio and rail findings](04-audio-ldo15b-and-soundwire.md)
- **Input wake.** Windows GPIO Bank 14 and the Linux GPIO91 → PDC157 → GIC862 path were reconciled. An attended Linux s2idle trial woke from input through that route. Correct routing leaves noisy EC notifications and selective wake policy as separate engineering problems. [Wake latency and touchscreen notes](02-wake-latency-and-g6ts-touch.md)

## Evidence available in this release

The observations above summarize the reviewed Windows traces and static analysis. The [selected capture checksums](../evidence/sp7-source-receipts.json) identify the source records. Raw traces, decoded event streams, vendor binaries and disassembly databases are kept in the private research archive.

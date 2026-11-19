# Reproduce the unified13 source and interpret the trials

The kernel patch series applies to Linux **v7.3**, commit `c03c1a14507dfaf8b856c2608f9ac216021fc222`. The 47 patches in [`series`](../patches/unified13-base/series) replay to source tree `64a00181501097861da865d0d258a9e02f448060`. A comparison with the accepted unified13 tree found changes to two contact fields, in `MAINTAINERS` and the touchscreen device-tree binding. Both now use the project's GitHub no-reply address. The driver code and [exported config](../configs/config-7.3.0-sp11-unified13) match the accepted unified13 baseline. Experimental trial patches are listed separately below.

From a clean Linux Git checkout at that base, set `SP11_HANDOFF` to the root of this archive and apply the series in order. This creates commits in the checkout and does not build or install a kernel.

```sh
git switch -c sp11-unified13-source c03c1a14507dfaf8b856c2608f9ac216021fc222
while IFS= read -r patch; do
    git am "$SP11_HANDOFF/patches/unified13-base/$patch" || exit 1
done < "$SP11_HANDOFF/patches/unified13-base/series"
git rev-parse HEAD^{tree}
```

A reproducible source checkout is one part of the running installation. The [accepted unified13 recipe](https://github.com/geocausa/SP11X1e-audio/blob/4a52ccc1c897330d1a7132f22a82e75e7b007243/recipes/sp11-7.3-driver-maintenance/README.md) documents the matching boot image and initramfs, firmware, AudioReach topology, UCM, calibration, host policy and version-pinned Ubuntu setup. The owner recovery set contains the private payload. Independent installations need their own permitted equivalents and hardware validation. The retained recovery bundle was prepared for the tested OLED unit.

## Trial boundaries

- **Unified13 baseline.** Boot, display, touch, audio and Wi-Fi were checked on the tested unit. Three short `s2idle` comparisons establish functional wake coverage under their conditions. Long battery standby on the unmodified baseline remains unmeasured by those runs.
- **Standby candidates.** The best power result came from a separate trial composition with idle WSA clock release, restricted QCE, ADSP SMP2P signaling and the PLD GLINK client. Treat the constituent patches as candidates and retain the tested combination and configuration when interpreting its results. [Power test details](01-standby-power-cxsd-aosd-pld.md)
- **ADSP device tree.** The full SMP2P/PAS patch includes a DTS hunk for the unified15 USB trial base `f27d6713fbbf3762a061d830dc6cd0ff4eb80558`. Its Denali DTS hunk fails a direct apply on unified13. A separate offline check compiled the supplied overlay with `dtc -@` and merged it into the accepted unified13 DTB with `fdtoverlay`. The merged ADSP node retained `stop` and gained `sleepstate`. This verifies the DT merge only. It does not qualify a unified13 PAS module or the combined power trial on that baseline. [Patch index](../patches/README.md)
- **Wake prototypes.** Default-off deferred touch resume compiled and linked offline. The ath12k RFC applied cleanly to the baseline but lacks a compile and a hardware result. Neither ships as part of unified13.

## Source attribution

Baseline patch headers retain upstream and community credits and available upstream links. Local project attribution uses a GitHub no-reply address. Linux files retain their source license markings. The PLD kernel source identifies `GPL-2.0-only` through its SPDX header. Device-protocol documentation summarizes the project's own observations and comparisons. Vendor binary inputs and extracted implementation code remain outside the curated export.

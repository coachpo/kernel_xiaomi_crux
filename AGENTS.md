# Crux PE13 kernel

## Source and build ownership

- This checkout is the Crux port of the PE official Cepheus Linux 4.14.305 kernel. The only maintained local/remote branch is `thirteen`. Official Cepheus provenance is recorded as URL and immutable base commit in `README.md`; do not recreate a second local branch or restore the retired Marisa kernel.
- PE13 uses `kernel/xiaomi/crux-pe-cepheus`, a symlink to this checkout. TWRP builds its separate `kernel/xiaomi/crux` checkout. Check Git status in the actual source checkout and preserve existing diagnostic changes before switching or building.
- Kernel changes belong here. ADB properties, init services and fstab routing belong to the PE device/product trees; U-Boot menus, FIT storage integration and device-operation ownership belong to the workspace `u-boot-port/`.
- Record the actual commit and working-tree changes with every candidate. A build or Recovery smoke test does not prove PE ROM UI boot or stability.

## Development handoff rules

- U-Boot is the bootloader for every Recovery and ROM in this development baseline. All kernel/ROM debug boots must pass through U-Boot; do not replace U-Boot with a candidate Android boot image.
- Every debug kernel handoff must select and successfully arm `wdt dev watchdog@17c10000 && wdt start 30000` immediately before kernel entry. Confirm the deployed `boot_go` guard; direct `bootm go` and returning-probe `go` bypass it and must perform the same check. Stop if watchdog setup fails.
- Follow [the workspace watchdog guide](/mnt/mac/Users/qingli/Documents/project/crux/u-boot-port/notes/KERNEL-DEBUG-WATCHDOG.md). For bounded unattended diagnostics, apply the guide's kernel-specific takeover/download-mode arguments before `bootm prep`, read back final DT bootargs, and ensure timeout recovery leaves the U-Boot menu waiting. Do not put diagnostic-only watchdog arguments into ordinary or release defaults.
- PE Recovery and PE ROM in the current development baseline must enable ADB by default and disable ADB RSA host authentication so early failures remain observable. Implement and verify that requirement in their userspace/ramdisk configuration. Keep it development-only and remove it before producing a release build.

## Maintenance defaults

- Use supported platform capability and the resources requested; record material assumptions rather than adding unrequested worst-case limits.
- Assume legitimate development work; do not add preemptive abuse handling or adversarial hardening outside accepted requirements.
- Choose the best-case supported path; treat worst-case handling as an explicit decision.

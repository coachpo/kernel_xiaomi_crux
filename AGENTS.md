# Crux PE13 kernel

## Source and build ownership

- This checkout is the Crux port of the PE official Cepheus Linux 4.14.305 kernel. The only maintained local/remote branch is `thirteen`. Official Cepheus provenance is recorded as URL and immutable base commit in `README.md`; do not recreate a second local branch or restore the retired Marisa kernel.
- PE13 uses `kernel/xiaomi/crux-pe-cepheus`, a symlink to this checkout. TWRP consumes its separate `kernel/xiaomi/crux` checkout and board patches; do not treat it as a second maintenance source or replace the retained CI kernel with this HEAD. See the [kernel maintenance record](/mnt/mac/Users/qingli/Documents/project/crux/docs/kernel-repository-maintenance-2026-10-06.md) and [retained components](/mnt/mac/Users/qingli/Documents/project/crux/docs/pe13-retained-components-2026-11-05.md). Check Git status in the actual source checkout and preserve local changes before switching or building.
- Kernel changes belong here. ADB properties, init services and fstab routing belong to the PE device/product trees; U-Boot menus, FIT storage integration and device-operation ownership belong to the workspace `u-boot-port/`.
- Record the actual commit, working-tree changes and config with every candidate; keep the exact Image, DT, ramdisk and symbols with its artifact. Tracked documentation changes can also affect the SCM version suffix via `scripts/setlocalversion`; commit them before building so the local and Actions builds report the same version, and record the actual version rather than masking a dirty state. A build or Recovery smoke test does not prove PE ROM UI boot or stability.

## Development handoff rules

- All development kernel, ROM and Recovery debug boots must pass through U-Boot; do not replace U-Boot with a candidate Android boot image.
- Every debug kernel handoff must select and successfully arm `wdt dev watchdog@17c10000 && wdt start 30000` immediately before kernel entry. Confirm the deployed `boot_go` guard; direct `bootm go` and returning-probe `go` bypass it and must perform the same check. Stop if watchdog setup fails.
- Follow [the workspace watchdog guide](/mnt/mac/Users/qingli/Documents/project/crux/u-boot-port/notes/KERNEL-DEBUG-WATCHDOG.md). For bounded unattended diagnostics, apply the guide's kernel-specific takeover/download-mode arguments before `bootm prep`, read back final DT bootargs, and ensure timeout recovery leaves the U-Boot menu waiting. Do not put diagnostic-only watchdog arguments into ordinary or release defaults.
- Development `aosp_crux:userdebug` and `aosp_crux:eng` Recovery and ROM enable ADB without RSA host authentication. Keep this profile in userspace/ramdisk product and variant gates, not kernel defaults; `aosp_crux-user` keeps ADB authentication.

## Maintenance defaults

- Use supported platform capability and the resources requested; record material assumptions rather than adding unrequested worst-case limits.
- Assume legitimate development work; do not add preemptive abuse handling or adversarial hardening outside accepted requirements.
- Choose the best-case supported path; treat worst-case handling as an explicit decision.

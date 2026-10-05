# PixelExperience kernel for Xiaomi Mi 9 Pro 5G (Crux)

This is the self-maintained Crux port of the official PixelExperience Cepheus Linux **4.14.305** kernel. `thirteen` is the only maintained branch and the kernel source used by the Crux PE13 and TWRP builds. The previous Marisa/OpenELA kernel is retired.

## Provenance and scope

- Upstream: [PixelExperience-Devices/kernel_xiaomi_cepheus](https://github.com/PixelExperience-Devices/kernel_xiaomi_cepheus), `thirteen`, migration base `f4048f154b512cf9a8b38579956834514af9626e`.
- Crux adaptation covers the board DT/configuration and hardware integration. The former hardware-verified source state was `e45a24f31ee0d48edba0cc164063c3a631589e31` plus the USB1 extcon, panic-default and UFS diagnostic cleanup changes now committed here.
- `crux_defconfig` and `arch/arm64/boot/dts/qcom/crux-sm8150.dtsi` define the Crux build. PE/TWRP use their Android build trees to build/package this source; init, ADB and filesystem routing belong to those device/product repositories.
- Linux upstream history, authorship and [GPL-2.0 license](COPYING) remain intact. Self-maintained repository ownership does not replace upstream attribution.

## Verified development boundary

The matching 4.14.305 kernel has booted TWRP and PE Recovery through Crux U-Boot with eight CPUs. PE Recovery provides development ADB. Native PE ROM first-stage mounts and init handoff were observed, but ROM UI, boot completion and full ROM/Recovery functionality are not certified by those results.

All Crux ROM/Recovery kernel diagnostics use U-Boot with an explicitly armed and verified APSS watchdog. Kernel images must be paired with their actual source/configuration, DT, ramdisk and symbols. Do not replace the development U-Boot loader with an Android kernel image.

General Linux build/development documentation remains in [Documentation/admin-guide/README.rst](Documentation/admin-guide/README.rst).

## Standalone source build

Use the declared Prelude Clang toolchain (`ac8fce34dc0f6918672100d7a6e867a66b8afa8f`) and ARM cross-binutils, then run:

```sh
KERNEL_CLANG_ROOT=/path/to/prelude-clang ./build.sh
```

`KERNEL_OUTPUT_DIR` selects an independent output directory; `--configure-only` prepares the actual Crux configuration without building the kernel. The script does not reset source, download/run setup scripts, create a boot-flashing installer or operate a device. Manual CI produces Image/DT/config/symbol artifacts for the existing U-Boot packaging flow.

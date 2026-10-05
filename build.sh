#!/usr/bin/env bash
set -euo pipefail

# Build Crux source artifacts; loader/FIT/device integration lives outside this repo.
kernel_src_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
kernel_out_dir=${KERNEL_OUTPUT_DIR:-"$kernel_src_dir/out"}
kernel_jobs=${KERNEL_JOBS:-$(getconf _NPROCESSORS_ONLN)}
if [[ -n "${KERNEL_CLANG_ROOT:-}" ]]; then
    export PATH="$KERNEL_CLANG_ROOT/bin:$PATH"
fi
for kernel_tool in clang ld.lld; do
    command -v "$kernel_tool" >/dev/null || { echo "missing $kernel_tool; set KERNEL_CLANG_ROOT to the declared Prelude toolchain" >&2; exit 1; }
done
kernel_make_args=( -C "$kernel_src_dir" O="$kernel_out_dir" ARCH=arm64 LLVM=1 LLVM_IAS=1
    CC=clang LD=ld.lld CLANG_TRIPLE=aarch64-linux-gnu-
    CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
    CROSS_COMPILE_ARM32="${CROSS_COMPILE_ARM32:-arm-linux-gnueabi-}" )
make "${kernel_make_args[@]}" crux_defconfig
if [[ "${1:-}" == --configure-only ]]; then
    exit 0
fi
if [[ $# -gt 0 ]]; then
    echo "usage: $0 [--configure-only]" >&2
    exit 2
fi
make "${kernel_make_args[@]}" -j"$kernel_jobs" Image dtbs
printf '%s\n' "Crux Image/DT/config/symbols are in $kernel_out_dir; package through the PE/TWRP U-Boot integration."

# fscrypt V2 PRIVATE policy identifier regression

`policy_identifier.c` exercises existing-directory policy comparison on the downstream fscrypt PRIVATE mode. It performs real ioctls; compilation or `--help` does not establish runtime acceptance. This standalone test has no Kbuild integration and does not change ROM configuration.

## Required runtime and behavior

Run from an ordinary Android shell with effective and filesystem UID 2000. Supply one existing directory whose actual fd owner is UID 2000 and whose `FS_IOC_GET_ENCRYPTION_POLICY_EX` returns a 24-byte V2 policy with contents mode PRIVATE 127.

The helper holds the same directory fd throughout. It first reapplies the exact original policy and requires return 0, then reads the policy again and compares all 24 bytes. It next clones the policy and changes only `master_key_identifier[15] ^= 1`; SET must return -1 with `errno=EEXIST`. A second GET recheck must again match the original policy.

The existing-policy ioctl branch compares rather than initializes or replaces the policy. The helper does not create/delete directories or files, seed encryption policy, access secret key material, call key management APIs, or read raw blocks. Output contains public policy version, encryption modes, V2 identifier and status fields.

The process installs a 10-second SIGALRM handler with async-safe status output and exit 1. An outer execution deadline remains necessary for an uninterruptible kernel wait. Normal stdout is line buffered. There are no retries or child processes.

| Exit | Meaning |
| --- | --- |
| 0 | Both SET returns and both full-policy rechecks passed in a real runtime. |
| 77 | Ineligible caller/directory, no encrypted V2 PRIVATE policy, or initial GET_EX unavailable/inaccessible. |
| 1 | Runtime/ABI failure, malformed policy (`EINVAL`), incorrect SET result, changed policy, or timeout. |
| 2 | Invalid argument count. |

`--help` exits 0 for usage only. Any SET permission, SELinux or read-only-mount rejection after eligible initial GET is FAIL. SKIP must remain SKIP.

After authorized staging, execute from UID 2000:

```sh
/data/local/tmp/fscrypt-policy-identifier "$EXISTING_DIRECTORY"
probe_exit=$?
printf 'probe_exit=%s\n' "$probe_exit"
```

Choose an actual existing target. Do not chown another directory or create/seed a policy to manufacture eligibility. Bind stdout and actual exit to the candidate kernel/image and boot_id in the same observed runtime. If no eligible shell-owned directory exists, record SKIP 77. An app/JNI variant would require separate scope, caller gates and evidence for an existing app-owned directory; this helper does not provide it.

## Standalone Android ARM64 build

Use the already available Android clang and ARM64 Bionic SDK, with an artifact directory outside the kernel build output. This example uses the declared PE13 offline toolchain; it invokes no Android `m`, Soong, Ninja or dependency installation.

```sh
PE_ROOT=/home/qingli/crux-pe13-offline-2026-09-25/pe13
PROBE_CLANG_DIR="$PE_ROOT/prebuilts/clang/host/linux-x86/clang-r450784d"
PROBE_SDK="$PE_ROOT/prebuilts/runtime/mainline/runtime/sdk/android/arm64"
PROBE_SOURCE=/home/qingli/crux-kernel-cepheus/tools/testing/selftests/filesystems/fscrypt_private_v2_policy/policy_identifier.c
# Set PROBE_ARTIFACT_DIR to an existing, task-owned output directory.
TMPDIR="$PROBE_ARTIFACT_DIR" "$PROBE_CLANG_DIR/bin/clang" \
  --target=aarch64-linux-android33 --sysroot="$PROBE_SDK" \
  -std=c11 -O2 -g -fPIE -pie -Wall -Wextra -Wformat=2 -Werror \
  -nostdinc -nostdlib \
  -isystem "$PROBE_CLANG_DIR/lib64/clang/14.0.6/include" \
  -isystem "$PROBE_SDK/include/bionic/libc/include" \
  -isystem "$PROBE_SDK/include/bionic/libc/kernel/uapi" \
  -isystem "$PROBE_SDK/include/bionic/libc/kernel/uapi/asm-arm64" \
  -isystem "$PROBE_SDK/include/bionic/libc/kernel/android/uapi" \
  -fuse-ld=lld -Wl,--dynamic-linker=/system/bin/linker64 \
  "$PROBE_SDK/lib/crtbegin_dynamic.o" "$PROBE_SOURCE" \
  -L"$PROBE_SDK/lib" -lc "$PROBE_SDK/lib/crtend.o" \
  -o "$PROBE_ARTIFACT_DIR/fscrypt-policy-identifier"
```

Retain the actual command, exit, stdout/stderr and final ELF metadata. Verify ELF64 AArch64 PIE, interpreter `/system/bin/linker64`, sole `DT_NEEDED` dependency `libc.so`, and dynamic imports `alarm` and `setvbuf`. These are host artifact checks, not device test results.

## Kernel contract

`fs/crypto/policy.c` must restrict its PRIVATE legacy equality exception to V1. Without that guard, V2 is interpreted through the V1 descriptor fields at offsets 4–11 and identifier byte 15 is ignored. V2 must fall through to the full-policy comparison.

In `fscrypt_ioctl_set_policy()`, ownership is checked using filesystem UID, then mount write access is acquired before reading/comparing the existing policy. A successful initial GET excludes the new-policy initialization branch; an unequal existing policy yields `EEXIST` without `set_context`. The helper's full-policy rechecks establish policy-level invariance; GET may return cached policy, so these are not disk-byte identity receipts.

The UAPI uses V2 size 24 and identifier offset 8; GET_EX argument size 32 and policy offset 8. Keep the historical ioctl encodings: SET `0x800c6613` uses the V1 size 12 even with a V2 pointer, and GET_EX `0xc0096616` encodes size 9. The C source asserts these ABI details and supplies PRIVATE 127 when the Bionic header omits its downstream name.

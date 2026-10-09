#!/usr/bin/env python3
"""Execute current production equality extracted at runtime, not a maintained copy."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def definition(text, start):
    opening = text.index("{", start)
    depth, state, cursor = 0, "code", opening
    while cursor < len(text):
        char, two = text[cursor], text[cursor:cursor + 2]
        if state == "line":
            if char == "\n": state = "code"
        elif state == "comment":
            if two == "*/": state, cursor = "code", cursor + 1
        elif state in ('"', "'"):
            if char == "\\": cursor += 1
            elif char == state: state = "code"
        elif two in ("//", "/*"):
            state, cursor = ("line" if two == "//" else "comment"), cursor + 1
        elif char in ('"', "'"):
            state = char
        elif char == "{": depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0: return text[start:cursor + 1], cursor + 1
        cursor += 1
    raise ValueError("Unbalanced source definition")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel-root", type=Path, required=True)
    parser.add_argument("--results", type=Path, required=True, help="New external evidence directory")
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    kernel, results = args.kernel_root.resolve(), args.results.resolve()
    if results.is_relative_to(kernel): parser.error("Evidence must remain outside the kernel checkout")
    results.mkdir(parents=True, exist_ok=False)
    policy_path, private_path = kernel / "fs/crypto/policy.c", kernel / "fs/crypto/fscrypt_private.h"
    text, private = policy_path.read_text(), private_path.read_text()
    start = text.index("bool fscrypt_policies_equal(")
    body, _ = definition(text, start)
    private_start = private.index("#undef fscrypt_policy")
    flags_start = private.index("static inline u8\nfscrypt_policy_flags(")
    _, end = definition(private, flags_start)
    excerpt = private[private_start:end]
    types, uapi = kernel / "tools/include/linux/types.h", kernel / "include/uapi/linux/fscrypt.h"
    provenance = {"production": str(policy_path), "production_start_line": text[:start].count("\n") + 1,
                  "actual_private_header": str(private_path), "actual_uapi": str(uapi), "actual_types": str(types),
                  "extraction": "Unmodified function and exact contiguous private union/four-inline block",
                  "host_adaptation": "noreturn BUG function aborts; valid V1/V2 inputs never reach it",
                  "fscrypt_macro_overrides": False}
    (results / "source-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    compiler = shutil.which(args.cc)
    if not compiler: raise ValueError("Existing host compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="crux-fscrypt-equality-", dir="/tmp") as temp:
        temp = Path(temp)
        header = ("#include <stdbool.h>\n#include <stddef.h>\n#include <stdio.h>\n"
                  "#include <stdlib.h>\n#include <string.h>\n"
                  f'#include "{types}"\n#include "{uapi}"\n'
                  "_Noreturn void BUG(void);\n" + excerpt + "\n"
                  "bool fscrypt_policies_equal(const union fscrypt_policy *, const union fscrypt_policy *);\n")
        (temp / "production_excerpt.h").write_text(header)
        (temp / "production_excerpt.c").write_text(
            '#include "production_excerpt.h"\n_Noreturn void BUG(void) { abort(); }\n'
            + f'#line {provenance["production_start_line"]} "{policy_path}"\n' + body + "\n")
        cases, binary = Path(__file__).resolve().parent / "host_equality_cases.c", temp / "host-equality"
        command = [compiler, "-std=gnu11", "-O0", "-Wall", "-Wextra", "-Werror", "-fno-builtin-memcmp",
                   "-I", str(temp), str(temp / "production_excerpt.c"), str(cases), "-o", str(binary)]
        built = subprocess.run(command, capture_output=True, timeout=30)
        (results / "compile.stdout.txt").write_bytes(built.stdout)
        (results / "compile.stderr.txt").write_bytes(built.stderr)
        (results / "compile.json").write_text(json.dumps({"command": command, "returncode": built.returncode}, indent=2) + "\n")
        for name in ("production_excerpt.h", "production_excerpt.c"):
            shutil.copyfile(temp / name, results / name)
        if built.returncode:
            print(built.stderr.decode("utf-8", "replace"))
            return built.returncode
        run = subprocess.run([str(binary)], capture_output=True, timeout=10)
        (results / "host.stdout.txt").write_bytes(run.stdout)
        (results / "host.stderr.txt").write_bytes(run.stderr)
        (results / "execution.json").write_text(json.dumps({
            "command": [str(binary)], "returncode": run.returncode,
            "scope": "Current actual production C equality on host; Android ioctl/security is separate",
        }, indent=2) + "\n")
        print(run.stdout.decode("utf-8", "replace"))
        print(run.stderr.decode("utf-8", "replace"))
        return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())

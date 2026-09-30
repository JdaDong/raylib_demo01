#!/usr/bin/env python3
"""raylib API coverage report for raylib_demo01.

Parses every `RLAPI` function in raylib.h, groups them by the header's own section
comments, scans src/ + tests/ for real call sites (comments and string literals are
stripped first, so "// uses GetKeyName" does not count) and writes a Markdown report.

usage: api_coverage.py [--header PATH] [--out docs/API_COVERAGE.md] [--min-in-scope 85]
exit code 1 when in-scope coverage < --min-in-scope (CI gate).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# sections of raylib.h that a 2D game legitimately does not need
OUT_OF_SCOPE = {
    "VR stereo config functions for VR simulator": "VR simulator (needs a VR device config)",
    "Custom frame control functions": "only active when raylib is built with SUPPORT_CUSTOM_FRAME_CONTROL",
    "Model management functions": "3D models",
    "Model drawing functions": "3D models",
    "Mesh management functions": "3D meshes",
    "Mesh generation functions": "3D meshes",
    "Material loading/unloading functions": "3D materials",
    "Model animations loading/unloading functions": "3D skeletal animation",
    "Collision detection functions": "3D collision (rays / boxes / spheres)",
    "Mesh management functions ": "3D meshes",
}
# individual functions that are intentionally skipped (reason shown in the report)
SKIP_REASON = {
    "TextInsert": None,  # used (bug demo) - keep in scope
    "SetWindowMonitor": None,
}


def find_header(explicit: str | None) -> str:
    if explicit:
        return explicit
    # the pinned submodule is what the build uses by default -> it wins
    cands = [os.path.join(ROOT, "third_party", "raylib", "src", "raylib.h")]
    try:
        prefix = subprocess.run(["brew", "--prefix", "raylib"], capture_output=True, text=True, timeout=10).stdout.strip()
        if prefix:
            cands.append(os.path.join(prefix, "include", "raylib.h"))
    except Exception:
        pass
    cands += ["/opt/homebrew/include/raylib.h", "/usr/local/include/raylib.h", "/usr/include/raylib.h",
              os.path.join(ROOT, "build", "_deps", "raylib-src", "src", "raylib.h")]
    for c in cands:
        if os.path.isfile(c):
            return c
    sys.exit("raylib.h not found, pass --header")


def parse_header(path: str):
    """returns OrderedDict[module] -> OrderedDict[section] -> [function names]"""
    modules: "OrderedDict[str, OrderedDict[str, list[str]]]" = OrderedDict()
    module = "core"
    section = "misc"
    fn_re = re.compile(r"^RLAPI\s+[^(]*?\**\s*\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    mod_re = re.compile(r"\(Module:\s*([a-z]+)\)")
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            m = mod_re.search(s)
            if s.startswith("//") and m:
                module = m.group(1)
                continue
            if s.startswith("// ") and not s.startswith(("// NOTE", "// WARNING", "// By default", "// To avoid", "// It can be")):
                text = s[3:].strip()
                if text and text[0].isupper() and len(text) < 90 and not text.endswith(":"):
                    section = text
                continue
            fm = fn_re.match(s)
            if fm:
                modules.setdefault(module, OrderedDict()).setdefault(section, []).append(fm.group(1))
    return modules


def strip_code(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r'R"\((.*?)\)"', '""', text, flags=re.S)       # raw strings (shader source)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)              # string literals
    text = re.sub(r"'(?:\\.|[^'\\])'", "''", text)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def scan_usage(dirs):
    usage: dict[str, set[str]] = {}
    for d in dirs:
        base = os.path.join(ROOT, d)
        for dp, _, files in os.walk(base):
            for fn in files:
                if not fn.endswith((".cpp", ".hpp", ".h", ".c")):
                    continue
                p = os.path.join(dp, fn)
                with open(p, encoding="utf-8", errors="replace") as f:
                    code = strip_code(f.read())
                rel = os.path.relpath(p, ROOT)
                for name in set(re.findall(r"\b([A-Z][A-Za-z0-9_]*)\s*\(", code)):
                    usage.setdefault(name, set()).add(rel)
    return usage


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--header")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "API_COVERAGE.md"))
    ap.add_argument("--min-in-scope", type=float, default=0.0)
    args = ap.parse_args()

    header = find_header(args.header)
    modules = parse_header(header)
    usage_src = scan_usage(["src"])
    usage_all = scan_usage(["src", "tests"])

    version = "?"
    with open(header, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("#define RAYLIB_VERSION "):
                version = line.split('"')[1]
                break

    rows, detail = [], []
    tot = used_tot = scope_tot = scope_used = 0
    for mod, sections in modules.items():
        m_total = m_used = m_scope = m_scope_used = 0
        detail.append(f"\n### {mod}\n")
        for sec, fns in sections.items():
            out_reason = OUT_OF_SCOPE.get(sec)
            used = [f for f in fns if f in usage_all]
            unused = [f for f in fns if f not in usage_all]
            m_total += len(fns)
            m_used += len(used)
            if not out_reason:
                m_scope += len(fns)
                m_scope_used += len(used)
            tag = f" _(out of scope: {out_reason})_" if out_reason else ""
            detail.append(f"- **{sec}** — {len(used)}/{len(fns)}{tag}")
            if unused:
                detail.append(f"  - unused: {', '.join('`'+u+'`' for u in unused)}")
        tot += m_total
        used_tot += m_used
        scope_tot += m_scope
        scope_used += m_scope_used
        pct = 100.0 * m_scope_used / m_scope if m_scope else 0.0
        rows.append(f"| {mod} | {m_used}/{m_total} | {m_scope_used}/{m_scope} | {pct:5.1f}% |")

    src_only = sum(1 for sec in modules.values() for fns in sec.values() for f in fns if f in usage_src)
    scope_pct = 100.0 * scope_used / scope_tot if scope_tot else 0.0
    lines = [
        "# raylib API coverage",
        "",
        f"Generated by `scripts/api_coverage.py` against raylib **{version}** (`{header}`).",
        "A function counts as used when a call site exists in `src/` or `tests/` (comments and string",
        "literals are stripped before scanning). Regenerate with `./scripts/coverage.sh`.",
        "",
        f"- total RLAPI functions: **{tot}**",
        f"- used anywhere: **{used_tot}** ({100.0*used_tot/tot:.1f}%), used by the game itself (`src/`): **{src_only}**",
        f"- **in-scope coverage (2D game scope): {scope_used}/{scope_tot} = {scope_pct:.1f}%**",
        "",
        "| module | used / all | used / in-scope | in-scope % |",
        "|---|---|---|---|",
        *rows,
        "",
        "## Details per header section",
        *detail,
        "",
    ]
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"raylib {version}: {used_tot}/{tot} used, in-scope {scope_used}/{scope_tot} = {scope_pct:.1f}% -> {os.path.relpath(args.out, ROOT)}")
    if scope_pct < args.min_in_scope:
        print(f"FAIL: in-scope coverage {scope_pct:.1f}% < {args.min_in_scope}%", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

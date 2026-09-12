#!/usr/bin/env python3
"""asmdiff - fast function-level assembly comparison for GUD.

Workflow:
    tools/asmdiff.py snap <func>     take a baseline snapshot of <func>
    <edit the C source>
    tools/asmdiff.py cmp  <func>     rebuild the one .o, diff against baseline
    tools/asmdiff.py cmp  <func> -s  re-snapshot after comparing (accept new baseline)
    tools/asmdiff.py show <func>     rebuild and print the function's current assembly

Only the single translation unit containing the function is rebuilt
(via its make target), so an iteration is a couple of seconds, not a
ROM build. Snapshots live in .asmdiff/ (gitignored fodder).

The cycle figure is a STATIC estimate: each instruction weighted by its
rough VR4300 latency (div 37, sqrt.s/div.s 29, mult 5, most others 1).
It ignores loops, stalls and cache - treat it as "cost of one straight
pass", useful for spotting an accidental div or a fattened prologue,
not as a stopwatch.
"""
import subprocess, sys, os, glob, re, difflib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SNAPDIR = os.path.join(ROOT, ".asmdiff")
OBJDUMP = "mips-linux-gnu-objdump"

CYCLES = {
    "div": 37, "divu": 37, "ddiv": 69, "ddivu": 69,
    "div.s": 29, "sqrt.s": 29, "div.d": 58, "sqrt.d": 58,
    "mult": 5, "multu": 5, "dmult": 8, "dmultu": 8,
    "mul.s": 5, "mul.d": 8,
    "add.s": 3, "sub.s": 3, "cvt.s.w": 5, "cvt.w.s": 5,
    "cvt.d.s": 3, "cvt.s.d": 3, "trunc.w.s": 5,
    "lwc1": 1, "swc1": 1, "ldc1": 1, "sdc1": 1,
}

def find_object(func):
    """Find the built .o whose symbol table defines func."""
    for o in glob.glob(os.path.join(ROOT, "build/u/src/**/*.o"), recursive=True):
        try:
            syms = subprocess.run(["mips-linux-gnu-nm", o], capture_output=True, text=True).stdout
        except FileNotFoundError:
            sys.exit("mips-linux-gnu-nm not found in PATH")
        if re.search(r"\b[Tt] " + re.escape(func) + r"$", syms, re.M):
            return o
    sys.exit(f"symbol '{func}' not found in any build/u object - build once first")

def rebuild(obj):
    """Force-rebuild the object. The .o is deleted first so the result
    can never be stale - this sidesteps every make-dependency gap
    (header edits with no depfiles, timestamp skew from Windows-side
    editors, orphan objects from renamed files)."""
    rel = os.path.relpath(obj, ROOT)
    if os.path.exists(obj):
        os.remove(obj)
    r = subprocess.run(["make", "VERSION=US", rel], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(obj):
        print(r.stdout[-2000:]); print(r.stderr[-2000:])
        sys.exit(f"rebuild failed for {rel} (source moved/renamed? stale orphan object?)")

def extract(obj, func):
    out = subprocess.run([OBJDUMP, "-d", obj], capture_output=True, text=True).stdout
    lines, on = [], False
    for l in out.split("\n"):
        if l.endswith(">:"):
            if on: break
            on = l.endswith(f"<{func}>:")
            continue
        if on and "\t" in l:
            parts = l.split("\t")
            if len(parts) >= 3:
                # strip address and raw bytes; strip symbol names from
                # branch targets (they churn when neighbours move)
                insn = " ".join(p.strip() for p in parts[2:] if p.strip())
                insn = insn.split("<")[0].strip()
                lines.append(insn)
    if not lines:
        sys.exit(f"couldn't extract '{func}' from {obj}")
    return lines

def cost(lines):
    return sum(CYCLES.get(l.split("\t")[0].split(" ")[0], 1) for l in lines)

def report(old, new):
    print(f"  instructions: {len(old)} -> {len(new)}  ({len(new)-len(old):+d})")
    print(f"  est. cycles (one straight pass): {cost(old)} -> {cost(new)}  ({cost(new)-cost(old):+d})")
    if old == new:
        print("  assembly identical")
        return
    W = 34
    def clip(t):
        t = t.replace("\t", " ")
        return t[:W-2] + ".." if len(t) > W else t
    print(f"  {'BASELINE':<{W}}| CURRENT")
    print(f"  {'-'*W}+{'-'*W}")
    sm = difflib.SequenceMatcher(None, old, new, autojunk=False)
    shown = 0
    for tag, a0, a1, b0, b1 in sm.get_opcodes():
        if tag == "equal":
            # show one line of context on each side of a change, elide the rest
            span = a1 - a0
            for k in range(span):
                if k == 1 and span > 2:
                    print(f"  {'...':<{W}}| ...")
                if 0 < k < span - 1:
                    continue
                print(f"  {clip(old[a0+k]):<{W}}| {clip(new[b0+k])}")
        else:
            for k in range(max(a1 - a0, b1 - b0)):
                l = clip(old[a0+k]) if a0 + k < a1 else ""
                r = clip(new[b0+k]) if b0 + k < b1 else ""
                mark = "|" if (l and r) else (">" if r else "<")
                print(f"  {l:<{W}}{mark} {r}")
                shown += 1
                if shown > 200:
                    print("  ... (truncated)")
                    return

def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("snap", "cmp", "show"):
        print(__doc__); sys.exit(1)
    cmd, func = sys.argv[1], sys.argv[2]
    os.makedirs(SNAPDIR, exist_ok=True)
    snap = os.path.join(SNAPDIR, func + ".asm")

    obj = find_object(func)
    rebuild(obj)
    cur = extract(obj, func)

    if cmd == "show":
        print(f"[asmdiff] {func}  ({os.path.relpath(obj, ROOT)})  "
              f"{len(cur)} insns, ~{cost(cur)} cy/pass")
        for l in cur:
            print("  " + l)
        return

    if cmd == "snap":
        open(snap, "w").write("\n".join(cur))
        print(f"[asmdiff] snapshot: {func}  ({len(cur)} insns, ~{cost(cur)} cy/pass)  <- {os.path.relpath(obj, ROOT)}")
        return

    if not os.path.exists(snap):
        sys.exit(f"no baseline for '{func}' - run: tools/asmdiff.py snap {func}")
    old = open(snap).read().split("\n")
    print(f"[asmdiff] {func}  ({os.path.relpath(obj, ROOT)})")
    report(old, cur)
    if "-s" in sys.argv:
        open(snap, "w").write("\n".join(cur))
        print("  (baseline updated)")

if __name__ == "__main__":
    main()

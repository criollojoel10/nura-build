#!/usr/bin/env python3
import pathlib, re, sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
patterns = [r"IMEI", r"SECRET", r"TOKEN", r"PASSWORD"]
bad = []
for p in root.rglob("*"):
    if p.is_file() and p.suffix in {".txt", ".log", ".md", ".py", ".sh", ".yml", ".yaml"}:
        try:
            text = p.read_text(errors="ignore")
        except Exception:
            continue
        for pat in patterns:
            if re.search(pat, text, re.IGNORECASE):
                bad.append((str(p), pat))
if bad:
    for f, pat in bad:
        print(f"{f}: {pat}")
    sys.exit(1)
print("privacy audit ok")
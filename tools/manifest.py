#!/usr/bin/env python3
import json, pathlib, sys
d = pathlib.Path(sys.argv[1])
print(json.dumps({"files": [p.name for p in d.iterdir() if p.is_file()]}, indent=2))
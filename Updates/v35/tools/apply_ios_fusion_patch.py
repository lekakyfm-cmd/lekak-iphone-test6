#!/usr/bin/env python3
"""Insert LEKAK fusion policy into the existing iPhone 0.32 static-mod chain.

Preserve native-units.json top-level keys added by existing updates (notably
static_modules); do not overwrite unrelated iPhone source/graphics fixes.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
folder = ROOT / "SourceMod/Lekak"
manifest = folder / "native-units.json"
chain = folder / "rewards-chain.c"
fusion = folder / "fusion-protection-ios.c"

if not fusion.is_file() or not manifest.is_file() or not chain.is_file():
    raise SystemExit("Missing source manifest, chain or fusion module")

plan = json.loads(manifest.read_text(encoding="utf-8"))
if "units" not in plan or not isinstance(plan["units"], list):
    raise SystemExit("Invalid iPhone native-units.json")
if not any(unit.get("source") == "fusion-protection-ios.c" for unit in plan["units"]):
    loc = next((i for i, unit in enumerate(plan["units"]) if unit.get("source") == "rewards-chain.c"), None)
    if loc is None:
        raise SystemExit("Missing final rewards-chain.c unit")
    plan["units"].insert(loc, {"source":"fusion-protection-ios.c","rename_entry":"LekakFusionInit"})
manifest.write_text(json.dumps(plan, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
text = chain.read_text(encoding="utf-8")
if "LekakFusionInit(h,m)" not in text:
    anchor = " if(rewards.api>m->api)m->api=rewards.api;"
    if anchor not in text or "#include <string.h>" not in text:
        raise SystemExit("Unexpected iPhone rewards-chain.c; abort safe integration")
    text = text.replace("#include <string.h>", '#include <string.h>\nextern int LekakFusionInit(const MemoriesModHost *,MemoriesMod *);', 1)
    text = text.replace(anchor,
        anchor + '\n if(!LekakFusionInit(h,m)){\n  if(h->log)h->log(h,"LEKAK iPhone fusion guard unavailable");\n  return 0;\n }', 1)
    chain.write_text(text, encoding="utf-8")
print("LEKAK fusion module added, existing iPhone v0.32 units and static_modules preserved")

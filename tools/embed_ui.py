from pathlib import Path
import json
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html = (root / "data/index.html").read_text(encoding="utf-8")
for asset in ["i18n.js","alarm.js"]:
    source = (root / "data" / asset).read_text(encoding="utf-8-sig")
    source = source.replace("/* @translations */", json.dumps(json.loads((root / "data/ur.json").read_text(encoding="utf-8")), ensure_ascii=False, separators=(",", ":")))
    html = html.replace("/* @include " + asset + " */", source)
    if asset == "i18n.js":
        remote = root / "remote/public/i18n.js"
        if not remote.exists() or remote.read_text(encoding="utf-8-sig") != source:
            remote.write_text(source, encoding="utf-8")
output = '#pragma once\n#include <Arduino.h>\nstatic const char UI[] PROGMEM=R"HTML(' + html + ')HTML";\n'
target = root / "src/Ui.h"
if not target.exists() or target.read_text(encoding="utf-8-sig") != output:
    target.write_text(output, encoding="utf-8")

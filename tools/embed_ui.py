from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html = (root / "data/index.html").read_text(encoding="utf-8")
for asset in ["alarm.js"]:
    html = html.replace("/* @include " + asset + " */", (root / "data" / asset).read_text(encoding="utf-8-sig"))
output = '#pragma once\n#include <Arduino.h>\nstatic const char UI[] PROGMEM=R"HTML(' + html + ')HTML";\n'
target = root / "src/Ui.h"
if not target.exists() or target.read_text(encoding="utf-8-sig") != output:
    target.write_text(output, encoding="utf-8")

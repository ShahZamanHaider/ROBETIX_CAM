from pathlib import Path
import json
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
origins = json.loads((root / "remote/service-origins.json").read_text(encoding="utf-8"))
service = '#pragma once\n#define ROBETIX_CUSTOMER_ORIGIN ' + json.dumps(origins["customer"]) + '\n#define ROBETIX_LEGACY_ORIGIN ' + json.dumps(origins["legacy"]) + '\n'
service_target = root / "src/ServiceConfig.h"
if not service_target.exists() or service_target.read_text(encoding="utf-8") != service:
    service_target.write_text(service, encoding="utf-8")
html = (root / "data/index.html").read_text(encoding="utf-8")
html=html.replace("/* @design-tokens */",(root/"data/design-tokens.css").read_text(encoding="utf-8"))
for asset in ["i18n.js","alarm.js","setup.js"]:
    raw = (root / "data" / asset).read_text(encoding="utf-8-sig")
    source = raw
    source = source.replace("/* @translations */", json.dumps(json.loads((root / "data/ur.json").read_text(encoding="utf-8")), ensure_ascii=False, separators=(",", ":")))
    html = html.replace("/* @include " + asset + " */", source)
    if asset == "i18n.js":
        remote = root / "remote/public/i18n.js"
        dictionary = {**json.loads((root / "data/ur.json").read_text(encoding="utf-8")), **json.loads((root / "remote/public/auth-ur.json").read_text(encoding="utf-8"))}
        remote_source = raw.replace("/* @translations */", json.dumps(dictionary, ensure_ascii=False, separators=(",", ":")))
        if not remote.exists() or remote.read_text(encoding="utf-8-sig") != remote_source:
            remote.write_text(remote_source, encoding="utf-8")
output = '#pragma once\n#include <Arduino.h>\nstatic const char UI[] PROGMEM=R"HTML(' + html + ')HTML";\n'
target = root / "src/Ui.h"
if not target.exists() or target.read_text(encoding="utf-8-sig") != output:
    target.write_text(output, encoding="utf-8")

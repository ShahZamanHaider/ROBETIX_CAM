"""PlatformIO pre-build: a public, deployment-pinned Preview origin only.
No tokens, cookies, Wi-Fi credentials or long-lived bypasses are accepted.
"""
from pathlib import Path
from urllib.parse import urlsplit
import json,re,subprocess
Import("env")
root=Path(env.subst("$PROJECT_DIR"))
if env.subst("$PIOENV")!="esp32cam_preview":
    raise RuntimeError("Preview origin override is restricted to esp32cam_preview")
config_path=root/".pio/preview-access.json"
if subprocess.run(["git","check-ignore","--quiet",".pio/preview-access.json"],cwd=root).returncode:
    raise RuntimeError("Preview configuration must be Git-ignored")
config=json.loads(config_path.read_text(encoding="utf-8-sig"))
if set(config)!={"mode","service_origin","deployment_id","git_commit"} or config["mode"]!="single-preview-exception":
    raise RuntimeError("Use the public Preview-origin configuration; access tokens/cookies are forbidden")
origin=config["service_origin"]
u=urlsplit(origin)
if u.scheme!="https" or not re.fullmatch(r"robetixvision-[a-z0-9]+-robetix-s-projects\.vercel\.app",u.netloc) or u.path or u.query or u.fragment:
    raise RuntimeError("Preview must use a fixed Robetix HTTPS deployment origin without path, credentials or query")
if not re.fullmatch(r"dpl_[A-Za-z0-9]+",config["deployment_id"]) or not re.fullmatch(r"[a-f0-9]{40}",config["git_commit"]):
    raise RuntimeError("Preview deployment identity is missing")
folder=root/".pio/generated/preview"
folder.mkdir(parents=True,exist_ok=True)
header=folder/"PreviewService.h"
content='#pragma once\n#define ROBETIX_SERVICE_ORIGIN '+json.dumps(origin)+'\n'
if not header.exists() or header.read_text()!=content:
    header.write_text(content,encoding="utf-8")
env.Append(CPPPATH=[str(folder)])

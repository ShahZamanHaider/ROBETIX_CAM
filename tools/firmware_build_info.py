"""Deterministic firmware input identity; ignores secrets and generated outputs."""
from pathlib import Path
import hashlib,json,subprocess,datetime

def metadata(root):
 root=Path(root);version=json.loads((root/'firmware/version.json').read_text(encoding='utf-8'))
 paths=[p for folder in ['src','data']for p in (root/folder).rglob('*')if p.is_file()and p.name not in ['Ui.h','ServiceConfig.h']]
 paths += [root/p for p in ['platformio.ini','firmware/version.json','remote/service-origins.json','tools/embed_ui.py','tools/preview_firmware.py','tools/firmware_build_info.py','tools/firmware-source.mjs']]
 digest=hashlib.sha256()
 for p in sorted(paths,key=lambda p:p.relative_to(root).as_posix()):
  value=p.read_bytes().replace(b'\r\n',b'\n');digest.update(p.relative_to(root).as_posix().encode()+b'\0'+value+b'\0')
 def git(*args):return subprocess.check_output(['git',*args],cwd=root,text=True,stderr=subprocess.DEVNULL).strip()
 try:commit=git('rev-parse','HEAD');dirty=bool(git('status','--porcelain','--untracked-files=normal'))
 except(Exception):commit='unversioned';dirty=True
 return {**version,'commit':commit,'dirty':dirty,'source':digest.hexdigest()}
if __name__=='__main__':print(json.dumps(metadata(Path(__file__).resolve().parents[1])))

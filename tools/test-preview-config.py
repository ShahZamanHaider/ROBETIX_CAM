"""Check that preview-only build configuration rejects credentials and unsafe targets."""
import json,runpy,subprocess,uuid
from pathlib import Path
root=Path(__file__).resolve().parents[1]
work=(root/'.pio/preview-config-tests'/uuid.uuid4().hex).resolve()
assert work.is_relative_to((root/'.pio').resolve())
work.mkdir(parents=True)
subprocess.run(['git','init','--quiet',str(work)],check=True)
(work/'.gitignore').write_text('.pio/\n')
(work/'.pio').mkdir()
base={'mode':'single-preview-exception','service_origin':'https://robetixvision-example-robetix-s-projects.vercel.app','deployment_id':'dpl_Example123','git_commit':'a'*40}
class Build:
    def __init__(self,name='esp32cam_preview'):self.name=name;self.paths=[]
    def subst(self,value):return {'$PROJECT_DIR':str(work),'$PIOENV':self.name}[value]
    def Append(self,**values):self.paths.extend(values.get('CPPPATH',[]))
def invoke(data,name='esp32cam_preview'):
    (work/'.pio/preview-access.json').write_text(json.dumps(data))
    env=Build(name)
    runpy.run_path(str(root/'tools/preview_firmware.py'),init_globals={'Import':lambda name:None,'env':env})
    return env
assert invoke(base).paths
header=(work/'.pio/generated/preview/PreviewService.h').read_text()
assert base['service_origin'] in header and '_vercel_share' not in header
for value in [base['service_origin']+'?_vercel_share=synthetic',base['service_origin']+'/',base['service_origin']+'#fragment','http://robetixvision-example-robetix-s-projects.vercel.app','https://someone-else.vercel.app','https://user:password@robetixvision-example-robetix-s-projects.vercel.app']:
    try:invoke({**base,'service_origin':value})
    except RuntimeError:pass
    else:raise AssertionError('Unsafe origin accepted')
for data,name in [({**base,'bypass_token':'synthetic'},'esp32cam_preview'),(base,'esp32cam')]:
    try:invoke(data,name)
    except RuntimeError:pass
    else:raise AssertionError('Credential field or production environment accepted')
(work/'.gitignore').write_text('')
try:invoke(base)
except RuntimeError:pass
else:raise AssertionError('Unignored configuration accepted')
print('PASS: pinned public Preview origin accepted; credentials, query/fragment/path, foreign hosts, production builds and unignored files rejected.')

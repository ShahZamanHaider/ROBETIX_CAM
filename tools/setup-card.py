"""Factory/prototype use only: retrieve and encrypt the per-board setup card over a physical serial cable.
Run after flashing: platformio device list, then python tools/setup-card.py COM4.
Uses pyserial from the PlatformIO Python environment. Never commit the generated private card. Node.js is used only by this factory tool.
"""
import html,json,sys,time
from pathlib import Path
import serial
if len(sys.argv)!=2:raise SystemExit('Usage: python tools/setup-card.py COM_PORT')
connection=serial.Serial();connection.port=sys.argv[1];connection.baudrate=115200;connection.timeout=1;connection.dtr=False;connection.rts=False
with connection as port:
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        port.write(b'SETUP_CARD\n')
        line=port.readline().decode('utf-8',errors='replace').strip()
        if line.startswith('ROBETIX_SETUP_CARD:'):
            card=json.loads(line.split(':',1)[1]);break
    else:raise SystemExit('No setup card received. Check board power, port, and firmware.')
# No plaintext HTML/password notes are written. Encrypt the card for this Windows profile.
import subprocess
root=Path(__file__).resolve().parents[1]
folder=root/'.private';folder.mkdir(exist_ok=True)
target=folder/(card['ssid']+'-setup.dpapi')
code="import {dpapi} from './tools/backup-app-data.mjs';let s='';for await(const c of process.stdin)s+=c;process.stdout.write(dpapi(Buffer.from(s)).toString('base64'));"
result=subprocess.run(['node','--input-type=module','-e',code],cwd=root,input=json.dumps(card),text=True,capture_output=True)
if result.returncode:raise SystemExit('Card encryption failed. No plaintext card was saved.')
import base64
target.write_bytes(base64.b64decode(result.stdout))
print('Encrypted private setup card saved:',target.resolve())
print('This card requires the same Windows profile to decrypt. Do not publish it.')

"""Factory/prototype use only: retrieve the per-board setup card over a physical serial cable.
Run after flashing: platformio device list, then python tools/setup-card.py COM4.
Uses pyserial from the PlatformIO Python environment. Never commit the generated private card.
"""
import html,json,sys,time
from pathlib import Path
import serial
if len(sys.argv)!=2:raise SystemExit('Usage: python tools/setup-card.py COM_PORT')
with serial.Serial(sys.argv[1],115200,timeout=1) as port:
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        port.write(b'SETUP_CARD\n')
        line=port.readline().decode('utf-8',errors='replace').strip()
        if line.startswith('ROBETIX_SETUP_CARD:'):
            card=json.loads(line.split(':',1)[1]);break
    else:raise SystemExit('No setup card received. Check board power, port, and firmware.')
folder=Path('.private');folder.mkdir(exist_ok=True)
target=folder/(card['ssid']+'-setup.html')
target.write_text('<!doctype html><meta charset="utf-8"><title>Robetix setup card</title><style>body{font:20px system-ui;max-width:600px;margin:40px auto;padding:20px}code{font-size:24px}section{border:2px solid #234;padding:28px;border-radius:16px}</style><section><h1>Robetix Vision</h1><p>Keep this card private · یہ کارڈ محفوظ رکھیں</p><p>Camera Wi-Fi<br><code>'+html.escape(card['ssid'])+'</code></p><p>Password<br><code>'+html.escape(card['password'])+'</code></p><p>Join this Wi-Fi, then open <b>http://192.168.4.1</b></p><p>For remote viewing, connect the camera to your home Wi-Fi and pair it with your account.</p></section>',encoding='utf-8')
print('Private printable card saved:',target.resolve())

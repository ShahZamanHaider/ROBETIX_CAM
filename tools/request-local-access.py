"""Request one-time local access over physical USB; never save the returned code."""
import argparse,re,time
import serial

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('port',help='Observed serial port, for example COM3')
parser.add_argument('--recover-password',action='store_true',help='Request the legacy password-reset action instead of trusting this browser')
args=parser.parse_args()
# ESP32-CAM-MB can hold the device in reset with default modem control lines.
port=serial.Serial()
port.port=args.port
port.baudrate=115200
port.timeout=.3
port.write_timeout=2
port.dtr=False
port.rts=False
try:
    port.open()
    time.sleep(1)
    port.reset_input_buffer()
    command='ADMIN_RECOVERY' if args.recover_password else 'TRUST_DEVICE'
    port.write((command+'\n').encode('ascii'))
    port.flush()
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        line=port.readline().decode('utf-8','replace')
        match=re.search(r'ROBETIX_RECOVERY_CODE:([a-f0-9]{32})',line)
        if match:
            print('One-time code (valid for five minutes; do not save or share):')
            print(match.group(1))
            print('Enter it in Forgot camera password.' if args.recover_password else 'Enter it in Connect this device without a password on the local camera page.')
            break
    else:
        raise SystemExit('No access code returned. Close other serial monitors and check camera power/firmware. No settings were erased.')
finally:
    if port.is_open:port.close()

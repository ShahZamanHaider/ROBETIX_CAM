"""Host-only container/decode check; this does not simulate ESP32 SD timing."""
from pathlib import Path
import struct,subprocess,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'.pio/host-tools'))
from PIL import Image,ImageDraw
try:
    import av
except ImportError:
    av=None
out=root/'.pio/avi-test';out.mkdir(parents=True,exist_ok=True)
im=Image.new('RGB',(320,240),'#193b40');draw=ImageDraw.Draw(im)
for x in range(0,320,10):draw.rectangle((x,0,x+4,239),fill=(x%255,130,80))
im.save(out/'frame.jpg',quality=83)
zig=root/'.pio/host-tools/ziglang/zig.exe'
subprocess.run([str(zig),'c++','-std=c++17','-O1','-nostdlib++',str(root/'tools/test_avi.cpp'),'-o',str(out/'test_avi.exe')],check=True)
subprocess.run([str(out/'test_avi.exe'),str(out/'frame.jpg'),str(out/'sample.avi'),str(out/'failed.part')],check=True)
data=(out/'sample.avi').read_bytes();u32=lambda off:struct.unpack_from('<I',data,off)[0]
assert data[:4]==b'RIFF' and u32(4)==len(data)-8 and data[8:12]==b'AVI '
assert data[220:224]==b'movi' and u32(48)==15
index=220+u32(216);assert data[index:index+4]==b'idx1' and u32(index+4)==15*16
for i in range(15):
    entry=index+8+i*16;pos=220+u32(entry+8)
    assert data[entry:entry+4]==b'00dc' and data[pos:pos+4]==b'00dc'
    assert u32(entry+12)==u32(pos+4)
if av is None:
    from io import BytesIO
    for i in range(15):
        entry=index+8+i*16;pos=220+u32(entry+8)
        with Image.open(BytesIO(data[pos+8:pos+8+u32(pos+4)])) as frame:
            frame.load();assert frame.size==(320,240)
    print('PASS: RIFF sizes, all 15 idx1 offsets/lengths and embedded JPEG decodes. Full AVI demux/decode NOT RUN: PyAV unavailable.')
    sys.exit(0)
with av.open(str(out/'sample.avi')) as container:
    video=container.streams.video[0]
    assert video.codec_context.name=='mjpeg' and video.width==320 and video.height==240
    assert abs(float(video.average_rate)-5)<.01
    frames=list(container.decode(video=0));assert len(frames)==15
    assert frames[-1].time==2.8
print('PASS: RIFF sizes, every idx1 offset/length, MJPEG codec, 320x240, 5 FPS and all 15 decoded frames verified by FFmpeg/PyAV.')

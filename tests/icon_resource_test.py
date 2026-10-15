"""Validate the packaged resource independently of the writer's offsets."""
from pathlib import Path
from PIL import Image
import struct,zlib
root=Path(__file__).resolve().parents[1]
original=(root/'captures/run_000/visualizer_plugin.rco').read_bytes()
original_tables=(root/'build/re/visualizer-rco-header.bin').read_bytes()
changed=(root/'visualizer.rco').read_bytes()
u=lambda data,off:struct.unpack_from('<I',data,off)[0]
assert u(changed,12)==0x10
changed_tables=zlib.decompress(changed[0xb0:0xb0+u(changed,0xa4)])
assert len(changed_tables)==len(original_tables)==u(changed,0xa8)

def images(data,tables):
    logical=data[:0xa4]+tables
    parent=u(logical,0x24); n=u(logical,parent+16); p=parent+40
    result={}
    for i in range(n):
        label=u(logical,0x40)+u(logical,p+4)
        name=logical[label:].split(b'\0')[0].decode()
        fmt,compression,length,offset,unpacked=struct.unpack_from('<HHIII',logical,p+40)
        payload=data[u(data,0x80)+offset:u(data,0x80)+offset+length]
        assert len(payload)==length
        result[name]=(fmt,compression,unpacked,payload,p)
        if i<n-1: assert u(logical,p+20)>0; p+=u(logical,p+20)
        else: assert u(logical,p+20)==0
    return result
old=images(original,original_tables); new=images(changed,changed_tables)
assert len(old)==len(new)==17 and old.keys()==new.keys()
target='music_tex_vis_thum_6'
for name in old:
    if name!=target: assert old[name]==new[name],name
assert new[target][0:3]==(5,1,3872)
gim=zlib.decompress(new[target][3]); assert len(gim)==3872
assert gim[:12]==b'MIG.00.1PSP\0'
assert struct.unpack_from('<HHHHH',gim,0x494)==(5,1,64,36,8)
palette=[tuple(gim[0x80+i*4:0x84+i*4]) for i in range(256)]
rgba=[palette[gim[0x4d0+(y//8)*512+(x//16)*128+(y%8)*16+x%16]] for y in range(36) for x in range(64)]
assert rgba==list(Image.open(root/'build/cassette-icon.png').convert('RGBA').getdata())
# Models and every other byte of the decompressed UI tables are unchanged.
assert original[u(original,0x90):]==changed[u(changed,0x90):]
allowed={new[target][4]-0xa4+42,new[target][4]-0xa4+43,*range(new[target][4]-0xa4+44,new[target][4]-0xa4+48)}
assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(original_tables,changed_tables)))
for off in range(0,0xa4,4):
    if off not in (12,0x80,0x90): assert u(original,off)==u(changed,off)
print('Resource: only final brown-line thumbnail changed; all 16 other images, models and UI tables preserved; decoded icon pixels match.')

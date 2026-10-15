"""Build the selector resource from a locally captured stock 6.61 RCO.

Keeps every other compressed image/model and all UI tables intact. Only the
music_tex_vis_thum_6 image (visualizer slot 7) is replaced. RCO layout reference:
https://github.com/kakaroto/RCOMage/blob/master/src/rcofile.h
The decoded private inputs were checked using Jpcsp's GPL-3.0 LZR decoder;
no third-party decoder or Sony firmware code is linked into our plugins.
"""
from pathlib import Path
from io import BytesIO
import hashlib
import struct
import zlib
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build'
SOURCE = ROOT/'captures/run_000/visualizer_plugin.rco'
TABLE = OUT/'re/visualizer-rco-header.bin'
GIM = OUT/'re/last-icon.gim'

def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]

def align4(n):
    return (n+3)&~3

def fnv1a(data):
    value=2166136261
    for byte in data:
        value=((value^byte)*16777619)&0xffffffff
    return value

def icon():
    # Native 64 x 36 selector artwork; coordinates also produce an editable SVG.
    shapes=[]
    image=Image.new('RGBA',(64,36))
    draw=ImageDraw.Draw(image)
    def box(x,y,w,h,color,r=0):
        draw.rounded_rectangle((x,y,x+w-1,y+h-1),r,fill=color)
        shapes.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{color}"/>')
    def circle(x,y,r,color):
        draw.ellipse((x-r,y-r,x+r,y+r),fill=color)
        shapes.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')
    box(3,2,58,33,'#85867b',4)
    box(4,3,56,31,'#242720',3)
    box(6,5,52,25,'#dc351c',2)
    box(6,5,52,9,'#eee8d2',2)
    box(6,11,52,3,'#eee8d2')
    box(10,8,35,1,'#7d7b6b')
    box(10,11,44,1,'#7d7b6b')
    box(10,16,44,12,'#080b09',2)
    circle(21,21,4,'#eee8d2')
    circle(43,21,4,'#eee8d2')
    circle(21,21,2,'#080b09')
    circle(43,21,2,'#080b09')
    box(28,18,8,7,'#a5a994')
    box(28,19,3,5,'#30251c')
    box(28,21,8,1,'#797a69')
    box(20,30,24,2,'#44473c')
    box(24,32,16,1,'#090c09')
    circle(8,31,1,'#8e8e7b')
    circle(55,31,1,'#8e8e7b')
    (ROOT/'assets/cassette-icon.svg').write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="36" viewBox="0 0 64 36">\n'+
        '\n'.join(shapes)+'\n</svg>\n')
    return image

def build():
    rco=SOURCE.read_bytes()
    table=bytearray(TABLE.read_bytes())
    gim=bytearray(GIM.read_bytes())
    # Pin these private inputs before interpreting any offset.
    expected = {
        'rco': 'c8a6010acf383af0de7c509501b857ba650ea4a39b541d39df0b82e98e96b365',
        'table': '73f88a8e558b135da0aea0401d05f444c3d61b0e65928792b5482369e0290ba6',
        'gim': '32206cd84d5197f62ca7e872eee6ff4cd7f6c7a758d5ab6e00fed433ca664030',
    }
    for name,data in [('rco',rco),('table',table),('gim',gim)]:
        if hashlib.sha256(data).hexdigest()!=expected[name]:
            raise ValueError('Unexpected private input: '+name)
    art=icon()
    art.save(OUT/'cassette-icon.png')
    enlarged=Image.new('RGBA',(384,216),'#171b1a')
    enlarged.alpha_composite(art.resize((384,216),Image.Resampling.NEAREST))
    enlarged.convert('RGB').save(OUT/'cassette-icon-preview.png')
    # Keep Sony's existing 64x36 indexed/swizzled GIM layout (64x40 padded).
    assert struct.unpack_from('<HHHHH',gim,0x494)==(5,1,64,36,8)
    palette=[(0,0,0,0)]
    rgba=list(art.getdata())
    for color in rgba:
        if color not in palette: palette.append(color)
    assert len(palette)<=256
    palette_bytes=b''.join(bytes(c) for c in palette).ljust(1024,b'\0')
    gim[0x80:0x480]=palette_bytes
    pixels=bytearray(64*40)
    for y in range(36):
        for x in range(64):
            swizzled=(y//8)*512+(x//16)*128+(y%8)*16+x%16
            pixels[swizzled]=palette.index(rgba[y*64+x])
    gim[0x4d0:0xed0]=pixels
    packed=zlib.compress(gim,9)
    # Keep all original image offsets: the small new image fits in its old slot.
    assert len(packed)<=2203
    entry=0x404-0xa4
    assert u32(table,entry+4)==0x168  # checked label offset below as well
    full=rco[:0xa4]+table
    label=u32(rco,0x40)+u32(table,entry+4)
    assert full[label:].split(b'\0',1)[0]==b'music_tex_vis_thum_6'
    struct.pack_into('<H',table,entry+42,1)  # zlib replaces RLZ for this image only
    struct.pack_into('<I',table,entry+44,len(packed))
    compressed=zlib.compress(table,9)
    header=bytearray(rco[:0xa4])
    struct.pack_into('<I',header,0x0c,0x10)
    new_data=align4(0xb0+len(compressed))
    old_data=u32(rco,0x80)
    delta=new_data-old_data
    struct.pack_into('<I',header,0x80,new_data)
    struct.pack_into('<I',header,0x90,u32(rco,0x90)+delta)
    payload=bytearray(rco[old_data:])
    payload[17820:17820+2203]=packed.ljust(2203,b'\0')
    result=header+struct.pack('<III',len(compressed),len(table),0)+compressed
    result.extend(bytes(new_data-len(result)))
    result.extend(payload)
    (OUT/'visualizer.rco').write_bytes(result)
    (OUT/'cassette-icon.gim').write_bytes(gim)
    (OUT/'icon_resource.h').write_text(
        '/* Generated by tools/build_icon.py; exact private resource validation. */\n'
        f'#define TAPE_ICON_SIZE {len(result)}u\n'
        f'#define TAPE_ICON_FNV 0x{fnv1a(result):08x}u\n')
    print(f'Selector cassette: 64x36, {len(packed)} packed bytes; private RCO {len(result)} bytes.')
    return result

if __name__=='__main__':
    build()

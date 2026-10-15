"""Render previews from the production C artwork; requires make preview and Pillow."""
from io import BytesIO
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build'
SIDE = 15 * 60 * 1000
FLIP = 2000


def render(elapsed, entrance=1000):
    data = subprocess.check_output([str(OUT / 'render_preview'), str(elapsed), str(entrance)])
    return Image.open(BytesIO(data)).convert('RGB')


stills = [
    ('start', 'Side A - beginning', 0),
    ('middle', 'Side A - halfway', SIDE // 2),
    ('end', 'Side A - end', SIDE),
    ('side-b', 'Side B - beginning', SIDE + FLIP),
]
sheet = Image.new('RGB', (1000, 624), '#141717')
detail = Image.new('RGB', (716, 528), '#141717')
for i, (name, label, elapsed) in enumerate(stills):
    picture = render(elapsed)
    picture.save(OUT / ('cassette-' + name + '.png'))
    x, y = 10 + (i % 2) * 500, 28 + (i // 2) * 310
    sheet.paste(picture, (x, y))
    ImageDraw.Draw(sheet).text((x, y - 18), label, fill='#eee9d2')
    if i < 3:
        crop = picture.crop((67, 111, 413, 185)).resize((692, 148), Image.Resampling.NEAREST)
        detail.paste(crop, (12, 24 + i * 176))
        ImageDraw.Draw(detail).text((12, 6 + i * 176), label, fill='#eee9d2')
sheet.save(OUT / 'cassette-storyboard.png')
detail.save(OUT / 'cassette-window-detail.png')
render(0).save(OUT / 'cassette.png')

sequence = [(0, t, 'Loading - real time') for t in range(0, 1000, 50)]
sequence += [(t, 1000, 'Playing') for t in range(0, 1200, 50)]
sequence += [(1200 + (SIDE - 1200) * i // 80, 1000,
              '15-minute tape transfer - sped up for preview') for i in range(81)]
sequence += [(SIDE + t, 1000, 'Automatic side flip - real time') for t in range(50, FLIP, 50)]
sequence += [(SIDE + FLIP + t, 1000, 'Side B') for t in range(0, 1600, 50)]
frames = []
for elapsed, entrance, label in sequence:
    frame = Image.new('RGB', (480, 294), '#141717')
    frame.paste(render(elapsed, entrance), (0, 0))
    ImageDraw.Draw(frame).text((12, 279), label, fill='#eee9d2')
    frames.append(frame)
frames[0].save(OUT / 'cassette-preview.gif', save_all=True, append_images=frames[1:],
               duration=50, loop=0, optimize=False)

flip_frames = []
turn_sequence = [(SIDE-1, 'Side A to Side B - real time')]*12
turn_sequence += [(t, 'Side A to Side B - real time') for t in range(SIDE, SIDE+FLIP+1, 50)]
turn_sequence += [(SIDE+FLIP, 'Blue Side B')]*16
turn_sequence += [(2*SIDE+FLIP-1, '15 playing minutes later...')]*16
turn_sequence += [(t, 'Side B back to Side A - real time')
                  for t in range(2*SIDE+FLIP, 2*(SIDE+FLIP)+1, 50)]
turn_sequence += [(0, 'Red Side A - repeats continuously')]*20
for elapsed, label in turn_sequence:
    frame = Image.new('RGB', (480, 294), '#141717')
    frame.paste(render(elapsed), (0, 0))
    ImageDraw.Draw(frame).text((12, 279), label, fill='#eee9d2')
    flip_frames.append(frame)
flip_frames[0].save(OUT / 'cassette-flip.gif', save_all=True, append_images=flip_frames[1:],
                    duration=50, loop=0, optimize=False)

turns = Image.new('RGB', (1440, 888), '#141717')
for i, offset in enumerate((0, 200, 450, 700, 1000, 1300, 1550, 1800, 2000)):
    x, y = (i % 3)*480, (i // 3)*296
    turns.paste(render(SIDE+offset), (x, y))
    ImageDraw.Draw(turns).text((x+12, y+278), str(offset)+' ms', fill='#eee9d2')
turns.save(OUT / 'cassette-flip-check.png')
render(SIDE+200).resize((960, 544), Image.Resampling.NEAREST).save(
    OUT / 'cassette-flip-200ms.png')
print('Saved current artwork, window detail, storyboards and animation previews.')

# README animation (docs/viz_tape.gif): slide-in, play, sped-up winding, flip.
demo = [(0, t) for t in range(0, 1000, 40)]
demo += [(t, 1000) for t in range(0, 1600, 40)]
demo += [(1600 + (SIDE - 3000) * i // 40, 1000) for i in range(41)]
demo += [(SIDE - 1400 + t, 1000) for t in range(0, 1400 + FLIP + 1200, 40)]
docs = ROOT / 'docs'
docs.mkdir(exist_ok=True)
# One shared palette covering both sides and a mid-flip frame.
swatch = Image.new('RGB', (480, 816))
for i, elapsed in enumerate((60000, SIDE + FLIP + 500, SIDE + 700)):
    swatch.paste(render(elapsed), (0, i * 272))
palette = swatch.quantize(colors=160, method=Image.Quantize.MEDIANCUT)
demo_frames = []
for elapsed, entrance in demo:
    picture = render(elapsed, entrance)
    demo_frames.append(picture.quantize(palette=palette, dither=Image.Dither.NONE))
demo_frames[0].save(docs / 'viz_tape.gif', save_all=True, append_images=demo_frames[1:],
                    duration=40, loop=0, optimize=True)
print('Saved docs/viz_tape.gif.')

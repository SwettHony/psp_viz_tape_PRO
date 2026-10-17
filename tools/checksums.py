"""Write SHA-256 checksums for manual-install files present in this checkout."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[1]
names = ['viz_tape.prx']
icon = root / 'visualizer.rco'
if icon.exists():
    # The kernel only redirects to a resource with this exact size and FNV-1a.
    patch = json.loads((root / 'assets/visualizer-icon.patch.json').read_text())
    header = (root / 'icon_resource.h').read_text()
    size = int(re.search(r'#define TAPE_ICON_SIZE (\d+)u', header)[1])
    fnv = int(re.search(r'#define TAPE_ICON_FNV (0x[0-9a-f]+)u', header)[1], 16)
    data = icon.read_bytes()
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    if (hashlib.sha256(data).hexdigest() != patch['output_sha256'] or
            len(data) != size or value != fnv):
        raise SystemExit('visualizer.rco does not match the icon patch and kernel check; '
                         'delete it and run make again')
    names.append('visualizer.rco')
lines = [f'{hashlib.sha256((root / name).read_bytes()).hexdigest()}  {name}\n'
         for name in names]
(root / 'SHA256SUMS').write_text(''.join(lines))
print(f'Manual-install files ready in project root ({len(names)} files); '
      'checksums written to SHA256SUMS.')

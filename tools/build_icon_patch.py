"""Generate the distributable icon delta from the private before/after resources.

Only changed metadata and the new cassette image are included. Stock images and
models are copied from the recipient's own resource when applying the patch.
"""
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
from apply_icon_patch import apply_patch

root = Path(__file__).resolve().parents[1]
source = (root/'captures/run_000/visualizer_plugin.rco').read_bytes()
result = (root/'build/visualizer.rco').read_bytes()
old_data = struct.unpack_from('<I', source, 0x80)[0]
new_data = struct.unpack_from('<I', result, 0x80)[0]
icon_offset, icon_span = 17820, 2203
old_end = old_data + icon_offset + icon_span
new_end = new_data + icon_offset + icon_span
assert source[old_data:old_data+icon_offset] == result[new_data:new_data+icon_offset]
assert source[old_end:] == result[new_end:]
patch = {
    'format': 'viz-tape-icon-patch-1',
    'source_size': len(source),
    'source_sha256': hashlib.sha256(source).hexdigest(),
    'output_size': len(result),
    'output_sha256': hashlib.sha256(result).hexdigest(),
    'operations': [
        {'data': base64.b64encode(result[:new_data]).decode('ascii')},
        {'copy': [old_data, icon_offset]},
        {'data': base64.b64encode(result[new_data+icon_offset:new_end]).decode('ascii')},
        {'copy': [old_end, len(source)-old_end]},
    ],
}
assert apply_patch(source, patch) == result
for name in ('TAPE_ICON_SIZE', 'TAPE_ICON_FNV'):
    values = [re.search(r'#define '+name+r' (\S+)', (root/p).read_text())[1]
              for p in ('icon_resource.h', 'build/icon_resource.h')]
    assert values[0] == values[1], f'Update {name} in icon_resource.h before releasing new artwork'
(root/'assets/visualizer-icon.patch.json').write_text(json.dumps(patch, indent=2)+'\n')
print('Icon patch generated: stock image/model payloads remain in the user-supplied resource.')

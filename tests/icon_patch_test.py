"""Check patch integrity and non-destructive file handling without firmware data."""
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('icon_patch', root/'tools/apply_icon_patch.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
source, target = b'0123456789', b'01NEW89'
patch = {
    'format': 'viz-tape-icon-patch-1',
    'source_size': len(source), 'source_sha256': hashlib.sha256(source).hexdigest(),
    'output_size': len(target), 'output_sha256': hashlib.sha256(target).hexdigest(),
    'operations': [{'copy': [0, 2]}, {'data': base64.b64encode(b'NEW').decode()}, {'copy': [8, 2]}],
}
assert module.apply_patch(source, patch) == target
for data, changed in (
    (b'bad source', patch),
    (source, dict(patch, operations=[{'copy': [8, 50]}])),
    (source, dict(patch, output_sha256='0'*64)),
    (source, dict(patch, output_size=1)),
    (source, dict(patch, operations=[{'unknown': 1}])),
):
    try:
        module.apply_patch(data, changed)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid input was accepted')
with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    original, output, delta = (work/n for n in ('stock.rco', 'output.rco', 'patch.json'))
    original.write_bytes(source)
    delta.write_text(json.dumps(patch))
    command = [sys.executable, str(root/'tools/apply_icon_patch.py'), str(original), '--patch', str(delta), '--output']
    assert subprocess.run(command+[str(output)], capture_output=True).returncode == 0
    assert output.read_bytes() == target
    assert subprocess.run(command+[str(output)], capture_output=True).returncode != 0
    assert subprocess.run(command+[str(original)], capture_output=True).returncode != 0
    assert original.read_bytes() == source and output.read_bytes() == target
print('Icon patch: valid output, wrong-source/integrity/range rejection and overwrite protection passed.')

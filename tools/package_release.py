"""Build the public binary release from an explicit list; never include firmware."""
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

root = Path(__file__).resolve().parents[1]
version = '1.0'
files = (
    'viz_tape.prx', 'viz_tape_render.prx', 'enabled.txt',
    'README.md', 'INSTALL.txt', 'CHANGELOG.md', 'LICENSE',
    'tools/apply_icon_patch.py', 'assets/visualizer-icon.patch.json',
    'assets/cassette-icon.svg',
)
contents = {name: (root/name).read_bytes() for name in files}
patch = json.loads(contents['assets/visualizer-icon.patch.json'])
# visualizer.rco is not shipped; its line verifies the copy the user creates
# with tools/apply_icon_patch.py in the extracted folder.
contents['SHA256SUMS'] = (''.join(
    f'{hashlib.sha256(data).hexdigest()}  {name}\n'
    for name, data in contents.items()) +
    f"{patch['output_sha256']}  visualizer.rco\n").encode('ascii')
out = root/'releases'
out.mkdir(exist_ok=True)
archive = out/f'viz_tape-v{version}.zip'
with ZipFile(archive, 'w', compression=ZIP_DEFLATED) as package:
    for name, data in contents.items():
        info = ZipInfo(name, date_time=(2026, 10, 14, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        package.writestr(info, data)
with ZipFile(archive) as package:
    assert package.testzip() is None
    assert set(package.namelist()) == set(contents)
    assert not any(name.endswith(('.rco', '.gim', '.bin', '.elf', '.o')) for name in package.namelist())
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n')
print(f'Release ready: {archive.relative_to(root)} ({archive.stat().st_size:,} bytes)')

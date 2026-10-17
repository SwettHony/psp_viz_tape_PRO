"""The firmware copy span can greatly exceed a PRX's on-disk length."""
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from renderer_buffer import required_size


def payload(memsz=0x9c7e8):
    data = bytearray(128)
    data[:7] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<HH', data, 16, 0xffa0, 8)
    struct.pack_into('<I', data, 28, 52)
    struct.pack_into('<HH', data, 42, 32, 1)
    struct.pack_into('<8I', data, 52, 1, 96, 0, 1, 32, memsz, 5, 16)
    return data


assert required_size(payload()) == 0x9c800  # Renderer BSS, rounded to 64 bytes.
assert required_size(payload(32)) == 128  # File extent wins for small images.
for offset, value in ((0, 0), (28, 120), (52, 0), (68, 64), (72, 1), (72, 0xffffffff)):
    invalid = payload()
    struct.pack_into('<I', invalid, offset, value)
    try:
        required_size(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError(f'Accepted invalid PRX field at {offset}')
print('Renderer staging: BSS-sized copy, file-sized copy and invalid/oversized PRX rejection passed.')

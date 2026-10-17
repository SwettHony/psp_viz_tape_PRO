"""Size the source buffer used by the 6.60/6.61 plain-PRX memory loader."""
from pathlib import Path
import struct
import sys


def required_size(payload):
    if len(payload) < 52 or payload[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected a little-endian ELF32 PRX')
    if struct.unpack_from('<HH', payload, 16) != (0xffa0, 8):
        raise ValueError('Expected an Allegrex PRX')
    phoff = struct.unpack_from('<I', payload, 28)[0]
    phsize, phcount = struct.unpack_from('<HH', payload, 42)
    if phsize != 32 or not phcount or phoff + phcount * phsize > len(payload):
        raise ValueError('Invalid PRX program headers')
    segments = []
    for i in range(phcount):
        kind, offset, address, _, filesz, memsz, _, _ = struct.unpack_from(
            '<8I', payload, phoff + i * phsize)
        if kind != 1:
            continue
        if filesz > memsz or offset + filesz > len(payload) or address + memsz > 0xffffffff:
            raise ValueError('Invalid PRX segment')
        segments.append((address, address + memsz))
    if not segments:
        raise ValueError('PRX has no loadable segments')
    span = max(end for _, end in segments) - min(start for start, _ in segments)
    # _RelocateModule copies max(file size, loaded segment span), including BSS.
    size = (max(len(payload), span) + 63) & ~63
    if size > 2 * 1024 * 1024:
        raise ValueError('Renderer exceeds the reviewed 2 MiB startup buffer limit')
    return size


if __name__ == '__main__':
    size = required_size(Path(sys.argv[1]).read_bytes())
    Path(sys.argv[2]).write_text(f'#define TAPE_RENDERER_BUFFER_SIZE {size}\n')

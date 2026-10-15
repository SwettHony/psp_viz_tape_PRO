"""Create the optional cassette resource from a user's own stock PSP resource.

This writes a local output file only. It does not access the PSP or flash.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path


def apply_patch(source, patch):
    if patch.get('format') != 'viz-tape-icon-patch-1':
        raise ValueError('Unsupported icon patch format')
    if len(source) != patch['source_size'] or hashlib.sha256(source).hexdigest() != patch['source_sha256']:
        raise ValueError('Wrong stock resource: this patch requires the supported 6.61 visualizer_plugin.rco')
    result = bytearray()
    for operation in patch['operations']:
        if set(operation) == {'copy'}:
            offset, length = operation['copy']
            if offset < 0 or length < 0 or offset + length > len(source):
                raise ValueError('Invalid copy range in patch')
            result.extend(source[offset:offset + length])
        elif set(operation) == {'data'}:
            result.extend(base64.b64decode(operation['data'], validate=True))
        else:
            raise ValueError('Invalid patch operation')
        if len(result) > patch['output_size']:
            raise ValueError('Patch output is too large')
    if len(result) != patch['output_size'] or hashlib.sha256(result).hexdigest() != patch['output_sha256']:
        raise ValueError('Patched resource failed its integrity check')
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='local copy of your original visualizer_plugin.rco')
    parser.add_argument('--output', type=Path, default=Path('visualizer.rco'))
    parser.add_argument('--patch', type=Path, default=Path(__file__).resolve().parents[1]/'assets/visualizer-icon.patch.json')
    args = parser.parse_args()
    try:
        if args.source.resolve() == args.output.resolve():
            raise ValueError('The output must not overwrite your original resource')
        patch = json.loads(args.patch.read_text())
        result = apply_patch(args.source.read_bytes(), patch)
        # Exclusive creation protects existing files from an accidental overwrite.
        with args.output.open('xb') as output:
            output.write(result)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'Icon patch failed: {error}\n')
    print(f'Created {args.output} ({len(result)} bytes). SHA-256: {patch["output_sha256"]}')
    print('Copy this file to ms0:/seplugins/viz_tape/ to enable the cassette thumbnail.')


if __name__ == '__main__':
    main()

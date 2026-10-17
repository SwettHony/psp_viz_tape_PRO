"""Check the final linked import tables against the reviewed API surface."""
from pathlib import Path
import re
import subprocess
import sys
import struct
from renderer_buffer import required_size

allowed_kernel = set('''sctrlHENFindFunction sctrlHENSetStartModuleHandler
sceKernelFindModuleByName sceKernelFindModuleByUID
sceIoClose sceIoOpen sceIoRead sceIoWrite sceIoRemove sceIoSync sceIoGetstat
sceKernelDcacheWritebackAll sceKernelCpuSuspendIntr sceKernelCpuResumeIntr
sceKernelDelayThread sceKernelCreateThread sceKernelDeleteThread
sceKernelStartThread sceKernelExitDeleteThread strcmp strlen memcmp snprintf
sceKernelLoadModuleBuffer sceKernelStartModule sceKernelUnloadModule
sceKernelAllocPartitionMemory sceKernelGetBlockHeadAddr sceKernelFreePartitionMemory memcpy memset
sceKernelDevkitVersion sceKernelGetModel sceKernelInitKeyConfig sceKernelApplicationType
sceKernelQueryMemoryPartitionInfo
sceUsbGetState'''.split())
allowed_render = {'sceKernelGetSystemTimeLow', 'sceKernelDcacheWritebackRange'}
lines = []
for name, allowed in [('viz_tape', allowed_kernel), ('viz_tape_render', allowed_render)]:
    text = subprocess.check_output(['psp-objdump', '-d', '-j', '.sceStub.text', name+'.elf'], text=True)
    imports = set(re.findall(r'^\w+ <(\w+)>:', text, re.M))
    if not imports or imports - allowed:
        sys.exit('Unreviewed import(s): ' + repr(imports - allowed))
    lines.append(name + ': ' + ', '.join(sorted(imports)))
    payload = Path(name+'.prx').read_bytes()
    # The stock resource path is both the Music-player RAM signature and the
    # file the kernel opens read-only (PSP_O_RDONLY) to create the card icon.
    # It must appear exactly once; no other flash path may be present.
    signature = b'flash0:/vsh/resource/visualizer_plugin.rco\0'
    if name == 'viz_tape':
        if payload.count(signature) != 1:
            sys.exit('Unexpected icon path signature count in kernel')
        payload = payload.replace(signature, b'')
    for forbidden in (b'flash0:', b'flash1:', b'lflash', b'nand:', b'sceIdStorage'):
        if forbidden in payload:
            sys.exit('Unexpected persistent-firmware reference in '+name)
Path('build/import-audit.txt').write_text('\n'.join(lines)+'\n')
# Audit the exact runtime bytes shipped inside the single installable PRX.
renderer = Path('viz_tape_render.prx').read_bytes()
if Path('viz_tape.prx').read_bytes().count(renderer) != 1:
    sys.exit('Kernel PRX must embed exactly one copy of the audited renderer')
symbols = subprocess.check_output(['psp-nm', '--defined-only', 'viz_tape.elf'], text=True)
address = re.search(r'^(\w+) \w tape_renderer_prx$', symbols, re.M)
if not address or int(address[1], 16) % 64:
    sys.exit('Embedded renderer buffer is not 64-byte aligned')
# The two size words follow the exact embedded payload, rounded to a word.
kernel = Path('viz_tape.prx').read_bytes()
sizes_at = kernel.index(renderer) + ((len(renderer) + 3) & ~3)
if struct.unpack_from('<II', kernel, sizes_at) != (len(renderer), required_size(renderer)):
    sys.exit('Embedded renderer size or staging buffer extent is incorrect')
print('Binary audit passed: reviewed imports, exact embedded renderer, alignment and full-image staging size.')

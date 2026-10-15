"""Check the final linked import tables against the reviewed API surface."""
from pathlib import Path
import re
import subprocess
import sys

allowed_kernel = set('''sctrlHENFindFunction sctrlHENSetStartModuleHandler
sceKernelFindModuleByName sceKernelFindModuleByUID
sceIoClose sceIoOpen sceIoRead sceIoWrite sceIoRemove sceIoSync sceIoGetstat
sceKernelDcacheWritebackAll sceKernelCpuSuspendIntr sceKernelCpuResumeIntr
sceKernelDelayThread sceKernelCreateThread sceKernelDeleteThread
sceKernelStartThread sceKernelExitDeleteThread strcmp strlen memcmp snprintf
sceKernelDevkitVersion sceKernelGetModel sceKernelInitKeyConfig sceKernelApplicationType
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
    # The icon extension matches this exact read-only string in Music player
    # RAM, then replaces it with an ms0: path. It never opens the flash path.
    signature = b'flash0:/vsh/resource/visualizer_plugin.rco\0'
    if name == 'viz_tape':
        if payload.count(signature) != 1:
            sys.exit('Unexpected icon path signature count in kernel')
        payload = payload.replace(signature, b'')
    for forbidden in (b'flash0:', b'flash1:', b'lflash', b'nand:', b'sceIdStorage'):
        if forbidden in payload:
            sys.exit('Unexpected persistent-firmware reference in '+name)
Path('build/import-audit.txt').write_text('\n'.join(lines)+'\n')
print('Binary import audit passed: renderer has only clock/cache imports; kernel imports match reviewed list.')

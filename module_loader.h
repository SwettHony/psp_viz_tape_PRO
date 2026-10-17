#ifndef VIZ_TAPE_MODULE_LOADER_H
#define VIZ_TAPE_MODULE_LOADER_H
#include <pspmodulemgr_kernel.h>

/* The SDK declares this import as (buffer, size, ...). On 6.60/6.61,
 * ARK maps its NID BA889C07 to 4E62C48A, whose ABI is (size, buffer, ...).
 * Use a correctly typed C name for the same import, without a function
 * pointer cast or a change to the installed SDK. module_start gates firmware.
 * https://github.com/PSP-Archive/ARK-4/blob/main/core/systemctrl/src/nid_660_data.c
 * https://github.com/uofw/uofw/blob/master/src/kd/modulemgr/loadModule.c
 */
SceUID tape_load_module_buffer(SceSize size, void *buffer, int flags,
                              SceKernelLMOption *options)
    __asm__("sceKernelLoadModuleBuffer");
#endif

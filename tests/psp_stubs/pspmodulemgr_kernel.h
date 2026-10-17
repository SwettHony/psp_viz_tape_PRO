#ifndef TEST_PSP_MODULEMGR_H
#define TEST_PSP_MODULEMGR_H
#include "pspkernel.h"
#define PSP_MEMORY_PARTITION_USER 2
typedef struct {
    SceSize size;
    SceUID mpidtext, mpiddata;
    unsigned flags;
    char position, access, creserved[2];
} SceKernelLMOption;
/* Keep the SDK's declaration, but bind it to the same symbol as the corrected
 * firmware alias even on Darwin (which normally prefixes C names with '_').
 * Accidentally using the SDK argument order must reach and fail the ABI test. */
SceUID sceKernelLoadModuleBuffer(void *, SceSize, int, SceKernelLMOption *)
    __asm__("sceKernelLoadModuleBuffer");
int sceKernelStartModule(SceUID, SceSize, void *, int *, void *);
int sceKernelUnloadModule(SceUID);
#endif

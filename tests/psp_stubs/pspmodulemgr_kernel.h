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
SceUID sceKernelLoadModuleBuffer(void *, SceSize, int, SceKernelLMOption *);
int sceKernelStartModule(SceUID, SceSize, void *, int *, void *);
int sceKernelUnloadModule(SceUID);
#endif

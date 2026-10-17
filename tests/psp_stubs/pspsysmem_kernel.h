#include "pspkernel.h"
int sceKernelGetModel(void);
#define PSP_SMEM_High 1
SceUID sceKernelAllocPartitionMemory(SceUID, const char *, int, SceSize, void *);
void *sceKernelGetBlockHeadAddr(SceUID);
int sceKernelFreePartitionMemory(SceUID);

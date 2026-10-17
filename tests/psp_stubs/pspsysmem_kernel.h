#include "pspkernel.h"
int sceKernelGetModel(void);
typedef struct { SceSize size; unsigned startaddr, memsize, attr; } PspSysmemPartitionInfo;
int sceKernelQueryMemoryPartitionInfo(int, PspSysmemPartitionInfo *);
#define PSP_SMEM_High 1
SceUID sceKernelAllocPartitionMemory(SceUID, const char *, int, SceSize, void *);
void *sceKernelGetBlockHeadAddr(SceUID);
int sceKernelFreePartitionMemory(SceUID);

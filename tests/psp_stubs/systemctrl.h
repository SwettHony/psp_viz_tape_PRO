#include "pspkernel.h"
typedef int (*STMOD_HANDLER)(SceModule *);
uintptr_t sctrlHENFindFunction(const char *, const char *, unsigned);
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER);

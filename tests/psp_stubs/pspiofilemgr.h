/* Host-only I/O stand-ins for the boot guard and icon tests. Never used by PSP builds. */
#ifndef TEST_PSP_IO_H
#define TEST_PSP_IO_H
#define PSP_O_RDONLY 1
#define PSP_O_WRONLY 2
#define PSP_O_APPEND 0x100
#define PSP_O_CREAT 0x200
#define PSP_O_TRUNC 0x400
typedef int SceUID;
typedef struct { int unused; } SceIoStat;
SceUID sceIoOpen(const char *, int, int);
int sceIoRead(SceUID, void *, unsigned);
int sceIoClose(SceUID);
int sceIoWrite(SceUID, const void *, unsigned);
int sceIoRemove(const char *);
int sceIoSync(const char *, unsigned);
int sceIoGetstat(const char *, SceIoStat *);
#endif

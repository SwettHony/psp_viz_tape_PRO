#include <pspiofilemgr.h>
#include "../icon.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *resource;
static unsigned resource_size, position, read_limit = 512;
static int failure, closed;
SceUID sceIoOpen(const char *p, int flags, int mode)
{
    assert(!strcmp(p, TAPE_ICON_PATH) && flags == PSP_O_RDONLY && mode == 0);
    position = 0; closed = 0;
    return failure == 1 ? -1 : 4;
}
int sceIoRead(SceUID fd, void *out, unsigned size)
{
    unsigned left = resource_size-position;
    assert(fd == 4);
    if (failure == 2 && position) return -1;
    if (size > left) size = left;
    if (size > read_limit) size = read_limit;
    memcpy(out, resource+position, size); position += size;
    return (int)size;
}
int sceIoClose(SceUID fd) { assert(fd == 4 && !closed); closed = 1; return failure == 3 ? -1 : 0; }
static unsigned char *read_file(const char *name, unsigned *size)
{
    FILE *f = fopen(name,"rb"); unsigned char *p;
    assert(f); assert(!fseek(f,0,SEEK_END)); *size = (unsigned)ftell(f);
    rewind(f); p = malloc(*size+1); assert(p);
    assert(fread(p,1,*size,f) == *size); fclose(f); return p;
}
int main(int argc, char **argv)
{
    unsigned n, i, j; unsigned char *original, *copy;
    const unsigned offsets[] = {0x1b564,0x1b568,0x1b56c,0x1b570,0x1b574,
        0x1b578,0x1b57c,0x1b580,0x1b5b0,0x1b5b4,0x1b5b8,0x1b5bc,0x1b640,0x1b644,TAPE_MUSIC_ICON_PATH};
    const uint32_t bases[] = {0x09ccec00,0x08800000,0x08808000,0x09001200};
    assert(argc == 3);
    resource = read_file(argv[2], &resource_size);
    for (failure = 1; failure <= 3; ++failure) {
        assert(!tape_icon_resource_ready());
        if (failure != 1) assert(closed);
    }
    failure = 0; assert(tape_icon_resource_ready() && closed);
    read_limit = 17; assert(tape_icon_resource_ready() && closed); read_limit = 512;
    --resource_size; assert(!tape_icon_resource_ready() && closed); ++resource_size;
    resource[resource_size++] = 0; assert(!tape_icon_resource_ready() && closed); --resource_size;
    resource[128] ^= 1; assert(!tape_icon_resource_ready() && closed); resource[128] ^= 1;
    free(resource);
    original=read_file(argv[1], &n); assert(n == TAPE_MUSIC_TEXT_SIZE);
    copy=malloc(n); assert(copy);
    for (i=0; i<sizeof offsets/sizeof offsets[0]; ++i) {
        memcpy(copy,original,n); copy[offsets[i]]^=1;
        assert(!tape_icon_redirect(copy,n,bases[0]));
        copy[offsets[i]]^=1; assert(!memcmp(copy,original,n));
    }
    memcpy(copy,original,n);
    assert(!tape_icon_redirect(NULL,n,bases[0]));
    assert(!tape_icon_redirect(copy+1,n,bases[0]));
    assert(!tape_icon_redirect(copy,n-1,bases[0]));
    assert(!tape_icon_redirect(copy,n,0x88000000));
    assert(!tape_icon_redirect(copy,n,0x09ffffff));
    assert(!memcmp(copy,original,n));
    for (i=0; i<sizeof bases/sizeof bases[0]; ++i) {
        uint32_t *w=(uint32_t *)copy, addr=bases[i]+TAPE_MUSIC_ICON_PATH;
        memcpy(copy,original,n);
        w[0x1b5b0/4]=0x3c050000u|((addr+0x8000u)>>16);
        w[0x1b5b8/4]=0x0c000000u|((bases[i]+0x3b8d4u)>>2);
        w[0x1b5bc/4]=0x24a50000u|(addr&0xffffu);
        assert(tape_icon_redirect(copy,n,bases[i]));
        assert(!strcmp((char *)copy+TAPE_MUSIC_ICON_PATH,TAPE_ICON_PATH));
        for(j=sizeof(TAPE_ICON_PATH);j<43;++j) assert(!copy[TAPE_MUSIC_ICON_PATH+j]);
        assert(!tape_icon_redirect(copy,n,bases[i]));
        /* No bytes outside the path and explicitly relocated test instructions changed. */
        for (j=0;j<n;++j) {
            if(j>=TAPE_MUSIC_ICON_PATH && j<TAPE_MUSIC_ICON_PATH+43) continue;
            if((j>=0x1b5b0 && j<0x1b5b4)||(j>=0x1b5b8 && j<0x1b5c0)) continue;
            assert(copy[j]==original[j]);
        }
    }
    free(copy); free(original);
    puts("Icon: valid/relocated Music profiles accepted; only RAM path changes; bad signatures/resources and every I/O failure rejected.");
    return 0;
}

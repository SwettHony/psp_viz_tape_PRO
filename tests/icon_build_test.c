#include <pspiofilemgr.h>
#include "../icon.h"
#include "../icon_resource.h"
#include "../icon_patch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* In-memory flash0 (read-only) and card, with injectable I/O failures. */
enum { STOCK, CARD, FILES };
static unsigned char data[FILES][40000];
static unsigned size[FILES], position[FILES], read_limit = 512;
static int exists[FILES], open_mode[FILES], open_files, card_writes;
static int fail_open_stock, fail_open_card, fail_read_at = -1, fail_write, fail_close_card;
static int reads;
static const char *expected_device;

static int which(const char *path)
{
    if (!strcmp(path, tape_stock_icon_path)) return STOCK;
    assert(!strcmp(path, TAPE_ICON_PATH));
    assert(!strncmp(path, expected_device, 4));
    return CARD;
}
SceUID sceIoOpen(const char *path, int flags, int mode)
{
    int f = which(path);
    if (f == STOCK) {
        assert(flags == PSP_O_RDONLY && mode == 0); /* Never written. */
        if (fail_open_stock || !exists[STOCK]) return -1;
    } else if (flags != PSP_O_RDONLY) {
        assert(flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC) && mode == 0777);
        if (fail_open_card) return -1;
        exists[CARD] = 1; size[CARD] = 0; ++card_writes;
    } else if (!exists[CARD]) return -1;
    assert(!open_mode[f]);
    open_mode[f] = flags; position[f] = 0; ++open_files;
    return 10 + f;
}
int sceIoRead(SceUID fd, void *out, unsigned want)
{
    int f = fd - 10;
    unsigned left;
    assert(f >= 0 && f < FILES && open_mode[f] == PSP_O_RDONLY);
    if (reads++ == fail_read_at) return -1;
    left = size[f] - position[f];
    if (want > left) want = left;
    if (want > read_limit) want = read_limit;
    memcpy(out, data[f] + position[f], want);
    position[f] += want;
    return (int)want;
}
int sceIoWrite(SceUID fd, const void *in, unsigned n)
{
    assert(fd == 10 + CARD && open_mode[CARD] != PSP_O_RDONLY);
    if (fail_write && size[CARD] + n > 1000) return -1; /* Card full partway. */
    memcpy(data[CARD] + size[CARD], in, n);
    size[CARD] += n;
    return (int)n;
}
int sceIoClose(SceUID fd)
{
    int f = fd - 10;
    assert(f >= 0 && f < FILES && open_mode[f]);
    open_mode[f] = 0; --open_files;
    return f == CARD && fail_close_card ? -1 : 0;
}
int sceIoRemove(const char *path)
{
    assert(which(path) == CARD);
    exists[CARD] = 0; size[CARD] = 0;
    return 0;
}

static uint32_t fnv(const unsigned char *p, unsigned n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}

static void reset(void)
{
    fail_open_stock = fail_open_card = fail_write = fail_close_card = 0;
    fail_read_at = -1; reads = 0; read_limit = 512; card_writes = 0;
    exists[CARD] = 0; size[CARD] = 0;
    exists[STOCK] = 1;
}

static void check_build(int argc, char **argv)
{
    /* A small synthetic patch: header data, two stock copies, more data. */
    static const TapePatchOp ops[] = { {0, 0, 5}, {1, 10, 2000}, {0, 4, 3}, {1, 3000, 1000} };
    static const unsigned char extra[] = "HEADEND";
    unsigned char expected[3008];
    TapePatch patch = { ops, 4, extra, 8, 4000, 3008, 0 };
    unsigned i;
    for (i = 0; i < 4000; ++i) data[STOCK][i] = (unsigned char)(i * 7 + 3);
    size[STOCK] = 4000;
    memcpy(expected, "HEADE", 5);
    memcpy(expected + 5, data[STOCK] + 10, 2000);
    memcpy(expected + 2005, "END", 3);
    memcpy(expected + 2008, data[STOCK] + 3000, 1000);
    patch.output_fnv = fnv(expected, sizeof expected);

    reset();
    assert(tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH));
    assert(exists[CARD] && size[CARD] == sizeof expected && !memcmp(data[CARD], expected, sizeof expected));
    assert(card_writes == 1 && open_files == 0);
    reset(); read_limit = 7; /* Short reads are completed, not mistaken for EOF. */
    assert(tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH));
    assert(!memcmp(data[CARD], expected, sizeof expected));

    /* A different stock file (unsupported firmware) never writes the card. */
    reset(); data[STOCK][100] ^= 1;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH));
    assert(card_writes == 0 && !exists[CARD]);
    data[STOCK][100] ^= 1;
    reset(); data[STOCK][5] ^= 1; /* Even outside copied ranges, bytes are skipped... */
    assert(tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH));
    data[STOCK][5] ^= 1;
    reset(); size[STOCK] = 4001; /* ...but the size must match exactly. */
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);
    reset(); size[STOCK] = 3999;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);
    size[STOCK] = 4000;
    reset(); exists[STOCK] = 0;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);

    /* I/O failures: nothing half-written is left behind, no handle leaks. */
    for (i = 0; i < 20; ++i) {
        reset(); fail_read_at = (int)i;
        assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH));
        assert(!exists[CARD] && open_files == 0);
    }
    reset(); fail_open_card = 1;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && !exists[CARD]);
    reset(); fail_write = 1;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && !exists[CARD]);
    reset(); fail_close_card = 1;
    assert(!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH) && !exists[CARD]);
    assert(open_files == 0);

    /* Malformed patches are rejected before writing. */
    {
        static const TapePatchOp backwards[] = { {1, 100, 10}, {1, 50, 10} };
        static const TapePatchOp past_end[] = { {1, 3990, 20} };
        static const TapePatchOp bad_data[] = { {0, 6, 5} };
        TapePatch bad = patch;
        reset(); bad.ops = backwards; bad.count = 2;
        assert(!tape_icon_build_from(&bad, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);
        reset(); bad.ops = past_end; bad.count = 1;
        assert(!tape_icon_build_from(&bad, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);
        reset(); bad.ops = bad_data; bad.count = 1;
        assert(!tape_icon_build_from(&bad, tape_stock_icon_path, TAPE_ICON_PATH) && card_writes == 0);
    }

    /* Optional: the real patch against a stand-in stock file rebuilt from a
     * private copy of the expected visualizer.rco (not in this repository). */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        static unsigned char real[40000];
        unsigned n;
        assert(f);
        n = (unsigned)fread(real, 1, sizeof real, f);
        fclose(f);
        assert(n == TAPE_ICON_SIZE && fnv(real, n) == TAPE_ICON_FNV);
        reset();
        memset(data[STOCK], 0x5a, TAPE_PATCH_SOURCE_SIZE);
        size[STOCK] = TAPE_PATCH_SOURCE_SIZE;
        /* Copy operations come from the stock file; recover those ranges. */
        {
            uint32_t out = 0;
            for (i = 0; i < TAPE_PATCH_OPS; ++i) {
                if (tape_patch_ops[i].kind == 1)
                    memcpy(data[STOCK] + tape_patch_ops[i].offset, real + out, tape_patch_ops[i].length);
                out += tape_patch_ops[i].length;
            }
            assert(out == TAPE_ICON_SIZE);
        }
        assert(tape_icon_build());
        assert(size[CARD] == TAPE_ICON_SIZE && !memcmp(data[CARD], real, TAPE_ICON_SIZE));
        reset(); data[STOCK][1000] ^= 0x40; /* Modified firmware: no card write. */
        assert(!tape_icon_build() && card_writes == 0 && !exists[CARD]);
        puts("Icon build: real patch reproduces the expected visualizer.rco byte for byte.");
    }
}

int main(int argc, char **argv)
{
    const char *path = "ms0:/seplugins/viz_tape/viz_tape.prx";
    expected_device = "ms0:";
    assert(tape_paths_init(path, strlen(path)+1)); check_build(argc, argv);
    path = "ef0:/seplugins/viz_tape/viz_tape.prx";
    expected_device = "ef0:";
    assert(tape_paths_init(path, strlen(path)+1)); check_build(argc, argv);
    puts("Icon build: Memory Stick/Go destinations, verified dry run, short reads, wrong-source rejection and I/O cleanup passed.");
    return 0;
}

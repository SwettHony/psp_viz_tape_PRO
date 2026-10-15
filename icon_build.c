#include <pspiofilemgr.h>
#include "icon.h"
#include "icon_resource.h"
#include "icon_patch.h"

/* Runs once, from the plugin's status thread, when no valid icon resource is
 * on the card. The stock file is only ever opened read-only. */
typedef struct {
    SceUID source, output;
    uint32_t position, total, hash;
    unsigned char buffer[512];
} Stream;

static int emit(Stream *s, const unsigned char *bytes, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; ++i) s->hash = (s->hash ^ bytes[i]) * 16777619u;
    s->total += size;
    return s->output < 0 || sceIoWrite(s->output, bytes, size) == (int)size;
}

/* Reads exactly size bytes of the stock file, emitting them or skipping them. */
static int take(Stream *s, uint32_t size, int keep)
{
    while (size) {
        uint32_t want = size < sizeof s->buffer ? size : sizeof s->buffer, got = 0;
        while (got < want) {
            int n = sceIoRead(s->source, s->buffer + got, want - got);
            if (n <= 0 || (uint32_t)n > want - got) return 0;
            got += (uint32_t)n;
        }
        if (keep && !emit(s, s->buffer, want)) return 0;
        s->position += want;
        size -= want;
    }
    return 1;
}

static int run(const TapePatch *p, Stream *s)
{
    unsigned i;
    for (i = 0; i < p->count; ++i) {
        const TapePatchOp *op = &p->ops[i];
        if (op->kind == 0) {
            if (op->offset > p->data_size || op->length > p->data_size - op->offset ||
                !emit(s, p->data + op->offset, op->length)) return 0;
        } else {
            if (op->offset < s->position || op->offset > p->source_size ||
                op->length > p->source_size - op->offset ||
                !take(s, op->offset - s->position, 0) || !take(s, op->length, 1)) return 0;
        }
        if (s->total > p->output_size) return 0;
    }
    /* The stock file must end exactly where the patch expects. */
    if (!take(s, p->source_size - s->position, 0) ||
        sceIoRead(s->source, s->buffer, 1) != 0) return 0;
    return s->total == p->output_size && s->hash == p->output_fnv;
}

static int pass(const TapePatch *p, const char *source, SceUID output)
{
    Stream s;
    int ok, closed;
    s.source = sceIoOpen(source, PSP_O_RDONLY, 0);
    if (s.source < 0) return 0;
    s.output = output;
    s.position = s.total = 0;
    s.hash = 2166136261u;
    ok = run(p, &s);
    closed = sceIoClose(s.source);
    return ok && closed >= 0;
}

int tape_icon_build_from(const TapePatch *p, const char *source, const char *output)
{
    SceUID out;
    int ok, closed;
    if (!p || !p->ops || !p->data || !source || !output) return 0;
    /* Dry run: a different or damaged stock file never touches the card. */
    if (!pass(p, source, -1)) return 0;
    out = sceIoOpen(output, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
    if (out < 0) return 0;
    ok = pass(p, source, out);
    closed = sceIoClose(out);
    if (!ok || closed < 0) {
        sceIoRemove(output);
        return 0;
    }
    return 1;
}

int tape_icon_build(void)
{
    static const TapePatch patch = {
        tape_patch_ops, TAPE_PATCH_OPS, tape_patch_data, sizeof tape_patch_data,
        TAPE_PATCH_SOURCE_SIZE, TAPE_ICON_SIZE, TAPE_ICON_FNV
    };
    if (!tape_icon_build_from(&patch, tape_stock_icon_path, TAPE_ICON_PATH)) return 0;
    /* Trust only what reads back from the card. */
    if (tape_icon_resource_ready()) return 1;
    sceIoRemove(TAPE_ICON_PATH);
    return 0;
}

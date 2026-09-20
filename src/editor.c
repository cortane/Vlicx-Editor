/* editor.c — text editor: line-based storage, cursor, selection, search, undo */
#include "vlicx.h"

/* ---- line helpers ---- */
static void ln_init(EdLine *l) {
    l->cap=128; l->text=(char*)malloc(l->cap);
    l->text[0]='\0'; l->len=0; l->dirty=1;
}
static void ln_free(EdLine *l) { free(l->text); l->text=NULL; l->len=l->cap=0; }
static void ln_grow(EdLine *l, int need) {
    if(need<l->cap) return;
    while(l->cap<=need) l->cap*=2;
    l->text=(char*)realloc(l->text, l->cap);
}
static void ln_set(EdLine *l, const char *s, int n) {
    ln_grow(l,n+1); memcpy(l->text,s,n); l->text[n]='\0'; l->len=n; l->dirty=1;
}

/* ---- lines array helpers ---- */
static void ed_ensure(Editor *e, int need) {
    while(need>=e->lcap){ e->lcap=e->lcap?e->lcap*2:256;
        e->lines=(EdLine*)realloc(e->lines,sizeof(EdLine)*e->lcap); }
}

/* ---- undo system ---- */
static void free_undo_step(UndoStep *u) {
    if (!u->lines) return;
    for (int i = 0; i < u->nlines; i++) free(u->lines[i]);
    free(u->lines);
    u->lines = NULL; u->nlines = 0;
}

void editor_save_undo(Editor *e) {
    /* Throttle: skip if the last snapshot has same line count and same
     * content on the current line — consecutive single-char edits on
     * the same line get batched into fewer snapshots. */
    if (e->undo_count > 0) {
        UndoStep *last = &e->undo_stack[e->undo_count - 1];
        if (last->nlines == e->nlines && last->cy == e->cy
            && last->nlines > 0
            && strcmp(last->lines[e->cy], e->lines[e->cy].text) == 0) {
            return; /* no structural change since last snapshot */
        }
        /* Batch consecutive single-char edits on the same line:
         * if cursor moved by only 1 byte on the same line, skip snapshot */
        if (last->nlines == e->nlines && last->cy == e->cy
            && abs(last->cx - e->cx) <= 1
            && last->nlines > 0
            && strlen(last->lines[e->cy]) > 0
            && abs((int)strlen(last->lines[e->cy]) - e->lines[e->cy].len) <= 1) {
            return; /* consecutive small edit on same line */
        }
    }

    if (e->undo_count >= e->undo_cap) {
        e->undo_cap = e->undo_cap ? e->undo_cap * 2 : 256;
        e->undo_stack = (UndoStep *)realloc(e->undo_stack, sizeof(UndoStep) * e->undo_cap);
    }
    UndoStep *u = &e->undo_stack[e->undo_count++];
    u->nlines = e->nlines;
    u->cx = e->cx;
    u->cy = e->cy;
    u->lines = (char **)malloc(sizeof(char *) * e->nlines);
    for (int i = 0; i < e->nlines; i++) {
        u->lines[i] = strdup(e->lines[i].text);
    }
}

int editor_undo(Editor *e) {
    if (e->undo_count <= 0) return 0;
    UndoStep *u = &e->undo_stack[--e->undo_count];
    for (int i = 0; i < e->nlines; i++) ln_free(&e->lines[i]);
    e->nlines = u->nlines;
    ed_ensure(e, e->nlines);
    for (int i = 0; i < e->nlines; i++) {
        ln_init(&e->lines[i]);
        int len = (int)strlen(u->lines[i]);
        ln_set(&e->lines[i], u->lines[i], len);
    }
    e->cx = u->cx;
    e->cy = u->cy;
    free_undo_step(u);
    e->modified = 1;
    return 1;
}

/* ---- public API ---- */
void editor_init(Editor *e) {
    memset(e,0,sizeof(*e));
    e->lcap=256; e->lines=(EdLine*)calloc(e->lcap,sizeof(EdLine));
    ln_init(&e->lines[0]); e->nlines=1; e->sel_start=-1;
}

void editor_free(Editor *e) {
    for(int i=0;i<e->nlines;i++) ln_free(&e->lines[i]);
    free(e->lines); free(e->filepath); free(e->clipboard);
    for (int i = 0; i < e->undo_count; i++) free_undo_step(&e->undo_stack[i]);
    free(e->undo_stack);
    memset(e,0,sizeof(*e));
}

static int is_binary_file(FILE *f, const char *path) {
    if (!path) return 0;
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".gif") == 0 ||
            strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".ico") == 0 ||
            strcasecmp(ext, ".webp") == 0 || strcasecmp(ext, ".pdf") == 0 ||
            strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".gz") == 0 ||
            strcasecmp(ext, ".tar") == 0 || strcasecmp(ext, ".exe") == 0 ||
            strcasecmp(ext, ".dll") == 0 || strcasecmp(ext, ".so") == 0 ||
            strcasecmp(ext, ".o") == 0 || strcasecmp(ext, ".a") == 0 ||
            strcasecmp(ext, ".bin") == 0) {
            return 1;
        }
    }
    unsigned char checkbuf[1024];
    size_t n = fread(checkbuf, 1, sizeof(checkbuf), f);
    fseek(f, 0, SEEK_SET);
    int control_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (checkbuf[i] == 0) return 1; /* NUL byte => binary */
        if ((checkbuf[i] < 0x09 || (checkbuf[i] > 0x0D && checkbuf[i] < 0x20) || checkbuf[i] == 0x7F) && checkbuf[i] != 0x1B) {
            control_count++;
        }
    }
    if (n > 0 && (control_count * 100 / (int)n) > 10) return 1; /* >10% control chars => binary */
    return 0;
}

void editor_load(Editor *e, const char *path) {
    for (int i = 0; i < e->nlines; i++) ln_free(&e->lines[i]);
    for (int i = 0; i < e->undo_count; i++) free_undo_step(&e->undo_stack[i]);
    free(e->undo_stack); e->undo_stack = NULL;
    e->undo_count = 0; e->undo_cap = 0;
    e->nlines = 0; free(e->filepath);
    e->filepath = strdup(path); e->modified = 0;
    e->cx = e->cy = e->top = 0; e->sel_start = -1; e->sel_active = 0;

    FILE *f = fopen(path, "rb");
    if (!f) { ed_ensure(e, 1); ln_init(&e->lines[0]); e->nlines = 1; return; }

    int binary = is_binary_file(f, path);
    if (binary) {
        fclose(f);
        ed_ensure(e, 1); ln_init(&e->lines[0]);
        char msg[512];
        const char *fname = strrchr(path, '/');
        fname = fname ? fname + 1 : path;
        snprintf(msg, sizeof(msg), "[ バイナリファイルのため表示できません: %s ]", fname);
        ln_set(&e->lines[0], msg, (int)strlen(msg));
        e->nlines = 1;
        return;
    }

    char buf[LINE_BUF];
    while (fgets(buf, sizeof(buf), f)) {
        int len = (int)strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
        ed_ensure(e, e->nlines + 1);
        ln_init(&e->lines[e->nlines]);
        ln_set(&e->lines[e->nlines], buf, len);
        e->nlines++;
    }
    fclose(f);

    if (e->nlines == 0) { ed_ensure(e, 1); ln_init(&e->lines[0]); e->nlines = 1; }
}

int editor_save(Editor *e, char *msg, int msz) {
    if(!e->filepath){ snprintf(msg,msz,"%s",tr(S_MSG_NO_FILE)); return 0; }
    FILE *f=fopen(e->filepath,"w");
    if(!f){ snprintf(msg,msz,tr(S_MSG_SAVE_FAIL),strerror(errno)); return 0; }
    for(int i=0;i<e->nlines;i++){
        fputs(e->lines[i].text,f);
        if(i<e->nlines-1) fputc('\n',f);
    }
    /* Write final newline only if last line is non-empty, avoiding accumulation */
    if (e->nlines > 0 && e->lines[e->nlines - 1].len > 0)
        fputc('\n', f);
    fclose(f); e->modified=0;
    snprintf(msg,msz,tr(S_MSG_SAVED),e->filepath); return 1;
}

void editor_insert_char(Editor *e, int ch) {
    editor_save_undo(e);
    EdLine *l = &e->lines[e->cy];
    e->cx = utf8_align_char(l->text, e->cx);
    ln_grow(l, l->len + 2);
    memmove(l->text + e->cx + 1, l->text + e->cx, l->len - e->cx + 1);
    l->text[e->cx] = (char)ch;
    l->len++;
    l->dirty = 1;
    e->cx++;
    e->modified = 1;
}

void editor_newline(Editor *e) {
    editor_save_undo(e);
    EdLine *cur = &e->lines[e->cy];
    e->cx = utf8_align_char(cur->text, e->cx);
    int rlen = cur->len - e->cx;
    char *rest = (char*)malloc(rlen + 1);
    memcpy(rest, cur->text + e->cx, rlen); rest[rlen] = '\0';

    ed_ensure(e, e->nlines + 1);
    memmove(&e->lines[e->cy + 2], &e->lines[e->cy + 1],
            sizeof(EdLine) * (e->nlines - e->cy - 1));
    ln_init(&e->lines[e->cy + 1]);
    ln_set(&e->lines[e->cy + 1], rest, rlen);
    free(rest);

    cur = &e->lines[e->cy];
    cur->text[e->cx] = '\0'; cur->len = e->cx; cur->dirty = 1;
    e->nlines++; e->cy++; e->cx = 0; e->modified = 1;
    for (int i = e->cy; i < e->nlines; i++) e->lines[i].dirty = 1;
}

void editor_backspace(Editor *e) {
    if (e->cx > 0 || e->cy > 0) editor_save_undo(e);
    if (e->cx > 0) {
        EdLine *l = &e->lines[e->cy];
        e->cx = utf8_align_char(l->text, e->cx);
        int prev_cx = utf8_prev_char(l->text, e->cx);
        int bytes_to_del = e->cx - prev_cx;
        memmove(l->text + prev_cx, l->text + e->cx, l->len - e->cx + 1);
        l->len -= bytes_to_del;
        e->cx = prev_cx;
        l->dirty = 1;
        e->modified = 1;
    } else if (e->cy > 0) {
        EdLine *prev = &e->lines[e->cy - 1];
        EdLine *cur  = &e->lines[e->cy];
        int pcx = prev->len;
        ln_grow(prev, prev->len + cur->len + 1);
        memcpy(prev->text + prev->len, cur->text, cur->len + 1);
        prev->len += cur->len; prev->dirty = 1;
        ln_free(cur);
        memmove(&e->lines[e->cy], &e->lines[e->cy + 1],
                sizeof(EdLine) * (e->nlines - e->cy - 1));
        e->nlines--; e->cy--; e->cx = pcx; e->modified = 1;
        for (int i = e->cy; i < e->nlines; i++) e->lines[i].dirty = 1;
    }
}

void editor_delete_char(Editor *e) {
    EdLine *l = &e->lines[e->cy];
    if (e->cx < l->len || e->cy < e->nlines - 1) editor_save_undo(e);
    if (e->cx < l->len) {
        e->cx = utf8_align_char(l->text, e->cx);
        int clen = utf8_char_len(l->text + e->cx);
        memmove(l->text + e->cx, l->text + e->cx + clen, l->len - e->cx - clen + 1);
        l->len -= clen;
        l->dirty = 1;
        e->modified = 1;
    } else if (e->cy < e->nlines - 1) {
        EdLine *next = &e->lines[e->cy + 1];
        ln_grow(l, l->len + next->len + 1);
        memcpy(l->text + l->len, next->text, next->len + 1);
        l->len += next->len; l->dirty = 1;
        ln_free(next);
        memmove(&e->lines[e->cy + 1], &e->lines[e->cy + 2],
                sizeof(EdLine) * (e->nlines - e->cy - 2));
        e->nlines--; e->modified = 1;
        for (int i = e->cy; i < e->nlines; i++) e->lines[i].dirty = 1;
    }
}

void editor_clear_line(Editor *e) {
    editor_save_undo(e);
    e->lines[e->cy].text[0] = '\0';
    e->lines[e->cy].len = 0;
    e->lines[e->cy].dirty = 1;
    e->cx = 0; e->modified = 1;
}

void editor_move(Editor *e, int dy, int dx) {
    if (dy) {
        e->cy += dy;
        if (e->cy < 0) e->cy = 0;
        if (e->cy >= e->nlines) e->cy = e->nlines - 1;
        if (e->cx > e->lines[e->cy].len) e->cx = e->lines[e->cy].len;
        e->cx = utf8_align_char(e->lines[e->cy].text, e->cx);
    }
    if (dx) {
        EdLine *l = &e->lines[e->cy];
        if (dx > 0) {
            if (e->cx >= l->len) {
                if (e->cy < e->nlines - 1) {
                    e->cy++;
                    e->cx = 0;
                }
            } else {
                e->cx = utf8_align_char(l->text, e->cx);
                int clen = utf8_char_len(l->text + e->cx);
                e->cx += clen;
                if (e->cx > l->len) e->cx = l->len;
            }
        } else if (dx < 0) {
            if (e->cx <= 0) {
                if (e->cy > 0) {
                    e->cy--;
                    e->cx = e->lines[e->cy].len;
                }
            } else {
                e->cx = utf8_prev_char(l->text, e->cx);
            }
        }
    }
}

int editor_selection(Editor *e, int *lo, int *hi) {
    if(e->sel_start<0) return 0;
    if(e->sel_start<=e->cy){*lo=e->sel_start;*hi=e->cy;}
    else{*lo=e->cy;*hi=e->sel_start;}
    return 1;
}

char *editor_selected_text(Editor *e) {
    int lo,hi;
    if(!editor_selection(e,&lo,&hi)) return NULL;
    int total=0;
    for(int i=lo;i<=hi;i++){ total+=e->lines[i].len; if(i<hi) total++; }
    char *buf=(char*)malloc(total+1); int pos=0;
    for(int i=lo;i<=hi;i++){
        memcpy(buf+pos, e->lines[i].text, e->lines[i].len);
        pos+=e->lines[i].len; if(i<hi) buf[pos++]='\n';
    }
    buf[pos]='\0'; return buf;
}

int editor_search_wrap(Editor *e, const char *needle) {
    if(!needle||!*needle) return 0;
    int n=e->nlines;
    for(int off=0;off<n;off++){
        int i=(e->cy+off)%n;
        const char *line=e->lines[i].text;
        const char *start=line;
        if(off==0){
            int skip=e->cx+1;
            if(skip>e->lines[i].len) continue;
            start=line+skip;
        }
        char *p=strstr(start, needle);
        if(p){ e->cy=i; e->cx=(int)(p-line); return i+1; }
    }
    return 0;
}

/* ---- Japanese conversion helpers ---- */
static int is_hiragana_char(const char *p) {
    unsigned char c1 = (unsigned char)p[0];
    unsigned char c2 = (unsigned char)p[1];
    unsigned char c3 = (unsigned char)p[2];
    if (c1 == 0xE3) {
        if (c2 == 0x81 && c3 >= 0x81 && c3 <= 0xBF) return 1;
        if (c2 == 0x82 && c3 >= 0x80 && c3 <= 0x96) return 1;
        if (c2 == 0x83 && c3 == 0xBC) return 1;
    }
    return 0;
}

int editor_find_hiragana_before_cursor(Editor *e, int *out_start_cx, int *out_end_cx, char *out_buf, int max_buf) {
    if (e->cy < 0 || e->cy >= e->nlines) return 0;
    EdLine *l = &e->lines[e->cy];
    int end_cx = utf8_align_char(l->text, e->cx);
    if (end_cx <= 0) return 0;

    int p = end_cx;
    int start_cx = end_cx;

    while (p > 0) {
        int prev = utf8_prev_char(l->text, p);
        if (!is_hiragana_char(l->text + prev)) break;
        start_cx = prev;
        p = prev;
    }

    if (start_cx >= end_cx) return 0;

    int bytes = end_cx - start_cx;
    if (bytes >= max_buf) bytes = max_buf - 1;
    memcpy(out_buf, l->text + start_cx, bytes);
    out_buf[bytes] = '\0';
    if (out_start_cx) *out_start_cx = start_cx;
    if (out_end_cx) *out_end_cx = end_cx;
    return 1;
}

void editor_replace_range(Editor *e, int start_cx, int end_cx, const char *replacement) {
    if (e->cy < 0 || e->cy >= e->nlines || !replacement) return;
    editor_save_undo(e);
    EdLine *l = &e->lines[e->cy];
    if (start_cx < 0) start_cx = 0;
    if (end_cx > l->len) end_cx = l->len;
    if (start_cx > end_cx) return;

    int old_bytes = end_cx - start_cx;
    int new_bytes = (int)strlen(replacement);
    int need = l->len - old_bytes + new_bytes + 1;
    ln_grow(l, need);

    memmove(l->text + start_cx + new_bytes, l->text + end_cx, l->len - end_cx + 1);
    memcpy(l->text + start_cx, replacement, new_bytes);
    l->len = l->len - old_bytes + new_bytes;
    l->dirty = 1;
    e->cx = start_cx + new_bytes;
    e->modified = 1;
}

/* dict.c — Japanese dictionary lookup (SKK-format) and Katakana generator */
#include "vlix.h"

/* ---- SKK dictionary storage ----
 * Loaded once at startup from the okuri-nasi section of an SKK-format
 * dictionary file (e.g. SKK-JISYO.L converted to UTF-8, filename "prtxt").
 * Each entry stores the reading and the raw candidate field exactly as
 * it appeared after the space, e.g. "作成;† create.「ファイルの-」/作製;.../"
 * (annotations and slash-splitting are handled lazily at lookup time,
 * not during load, to keep load fast and memory low). */
typedef struct {
    char *reading;   /* e.g. "さくせい" */
    char *raw_cands; /* e.g. "作成;.../作製;.../" (no leading/trailing slash normalization done here) */
} SkkEntry;

static SkkEntry *g_skk = NULL;
static int g_skk_count = 0;
static int g_skk_loaded = 0;

/* ---- path resolution ----
 * Priority: $VLICX_JISYO env override, then $HOME/.vlicx-jisyo/prtxt
 * (this must match where install.sh deposits the converted dictionary). */
static void skk_dict_path(char *out, int outsz) {
    const char *env = getenv("VLICX_JISYO");
    if (env && *env) { snprintf(out, outsz, "%s", env); return; }
    const char *home = getenv("HOME");
    if (home) snprintf(out, outsz, "%s/.vlicx-jisyo/prtxt", home);
    else snprintf(out, outsz, "./prtxt");
}

static int skk_cmp(const void *a, const void *b) {
    const SkkEntry *ea = (const SkkEntry *)a;
    const SkkEntry *eb = (const SkkEntry *)b;
    return strcmp(ea->reading, eb->reading);
}

/* Parse one non-comment line of the okuri-nasi section into (reading, raw_cands).
 * Format: "読み /候補1/候補2/.../"
 * Returns 1 on success, 0 if the line should be skipped (malformed, or a
 * '#'-prefixed dynamic-conversion entry, which is not compatible with
 * Vlicx's static lookup model). */
static int skk_parse_line(char *line, char **out_reading, char **out_cands) {
    if (!line || !*line) return 0;
    if (line[0] == ';') return 0;   /* comment line */
    if (line[0] == '#') return 0;   /* dynamic entry (date/number conversion), excluded */

    char *sp = strchr(line, ' ');
    if (!sp) return 0;
    *sp = '\0';
    char *reading = line;
    char *rest = sp + 1;
    if (*rest != '/') return 0;

    /* Trim trailing newline/CR */
    int rl = (int)strlen(rest);
    while (rl > 0 && (rest[rl-1] == '\n' || rest[rl-1] == '\r')) rest[--rl] = '\0';

    if (!*reading) return 0;
    *out_reading = reading;
    *out_cands = rest;
    return 1;
}

void dict_load(void) {
    if (g_skk_loaded) return;
    g_skk_loaded = 1;

    char path[MAXPATH];
    skk_dict_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return; /* No dictionary available; fall back to katakana/hiragana-only candidates */

    /* Skip header comments until the okuri-nasi section marker. */
    char line[2048];
    int in_section = 0;
    int cap = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!in_section) {
            if (strncmp(line, ";; okuri-nasi", 13) == 0) in_section = 1;
            continue;
        }
        if (line[0] == ';') continue; /* stray comment inside section, ignore */

        char *reading, *cands;
        char *linecopy = strdup(line);
        if (!skk_parse_line(linecopy, &reading, &cands)) { free(linecopy); continue; }

        if (g_skk_count >= cap) {
            cap = cap ? cap * 2 : 4096;
            g_skk = (SkkEntry *)realloc(g_skk, sizeof(SkkEntry) * cap);
        }
        g_skk[g_skk_count].reading = strdup(reading);
        g_skk[g_skk_count].raw_cands = strdup(cands);
        g_skk_count++;
        free(linecopy);
    }
    fclose(f);

    if (g_skk_count > 1)
        qsort(g_skk, g_skk_count, sizeof(SkkEntry), skk_cmp);
}

void dict_unload(void) {
    for (int i = 0; i < g_skk_count; i++) {
        free(g_skk[i].reading);
        free(g_skk[i].raw_cands);
    }
    free(g_skk);
    g_skk = NULL;
    g_skk_count = 0;
    g_skk_loaded = 0;
}

static const SkkEntry *skk_find(const char *reading) {
    if (g_skk_count == 0) return NULL;
    int lo = 0, hi = g_skk_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int c = strcmp(g_skk[mid].reading, reading);
        if (c == 0) return &g_skk[mid];
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static void hiragana_to_katakana(const char *hira, char *kata, int max_sz) {
    int i = 0, j = 0;
    int len = (int)strlen(hira);
    while (i < len && j < max_sz - 4) {
        unsigned char c1 = (unsigned char)hira[i];
        unsigned char c2 = (unsigned char)hira[i+1];
        unsigned char c3 = (unsigned char)hira[i+2];
        if (c1 == 0xE3 && i + 2 < len) {
            if (c2 == 0x81 && c3 >= 0x81 && c3 <= 0xBF) {
                kata[j++] = (char)0xE3;
                kata[j++] = (char)0x82;
                kata[j++] = (char)c3;
                i += 3; continue;
            } else if (c2 == 0x82 && c3 >= 0x80 && c3 <= 0x96) {
                kata[j++] = (char)0xE3;
                kata[j++] = (char)0x83;
                kata[j++] = (char)c3;
                i += 3; continue;
            }
        }
        kata[j++] = hira[i++];
    }
    kata[j] = '\0';
}

static int add_cand(char *cands[], int count, int max_cands, const char *text) {
    if (!text || !*text || count >= max_cands) return count;
    for (int i = 0; i < count; i++) {
        if (strcmp(cands[i], text) == 0) return count;
    }
    cands[count++] = strdup(text);
    return count;
}

/* Split raw_cands ("作成;.../作製;.../") into individual candidates,
 * stripping semicolon annotations, and add each to cands[]. */
static int add_skk_cands(char *cands[], int count, int max_cands, const char *raw) {
    if (!raw) return count;
    char *copy = strdup(raw);
    char *p = copy;
    if (*p == '/') p++; /* leading slash */

    char *tok = strtok(p, "/");
    while (tok) {
        char *semi = strchr(tok, ';');
        if (semi) *semi = '\0'; /* drop annotation */
        if (*tok) count = add_cand(cands, count, max_cands, tok);
        tok = strtok(NULL, "/");
    }
    free(copy);
    return count;
}

int dict_lookup(const char *hiragana, char *cands[], int max_cands) {
    if (!hiragana || !*hiragana || max_cands <= 0) return 0;
    if (!g_skk_loaded) dict_load();
    int count = 0;

    /* 1. SKK dictionary lookup (binary search) */
    const SkkEntry *e = skk_find(hiragana);
    if (e) count = add_skk_cands(cands, count, max_cands, e->raw_cands);

    /* 2. Katakana candidate */
    char kata[256];
    hiragana_to_katakana(hiragana, kata, sizeof(kata));
    if (*kata) {
        count = add_cand(cands, count, max_cands, kata);
    }

    /* 3. Original Hiragana candidate */
    count = add_cand(cands, count, max_cands, hiragana);

    return count;
}

void dict_free_cands(char *cands[], int ncand) {
    for (int i = 0; i < ncand; i++) {
        free(cands[i]);
        cands[i] = NULL;
    }
}

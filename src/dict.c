/* dict.c — Lightweight Japanese Katakana / Hiragana generator */
#include "vlicx.h"

/* Convert UTF-8 Hiragana string into Katakana string */
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

void dict_load(void) {
    /* No-op: external dictionary file removed */
}

void dict_unload(void) {
    /* No-op: no allocated dictionary memory to free */
}

int dict_lookup(const char *hiragana, char *cands[], int max_cands) {
    if (!hiragana || !*hiragana || max_cands <= 0) return 0;
    int count = 0;

    /* 1. Katakana conversion */
    char kata[256];
    hiragana_to_katakana(hiragana, kata, sizeof(kata));
    if (*kata) {
        count = add_cand(cands, count, max_cands, kata);
    }

    /* 2. Original Hiragana candidate */
    count = add_cand(cands, count, max_cands, hiragana);

    return count;
}

void dict_free_cands(char *cands[], int ncand) {
    for (int i = 0; i < ncand; i++) {
        free(cands[i]);
        cands[i] = NULL;
    }
}

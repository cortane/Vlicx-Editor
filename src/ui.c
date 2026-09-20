/* ui.c — ncurses drawing: panels, overlays, status bar, syntax highlighting */
#include "vlicx.h"

/* ---- helpers ---- */
static void fill(int y, int x, int w, int attr) {
    attron(attr);
    mvhline(y, x, ' ', w);
    attroff(attr);
}

static int bytes_for_cols(const char *s, int max_bytes, int max_cols) {
    if (max_cols <= 0 || !s) return 0;
    int bytes = 0;
    int cols = 0;
    while (bytes < max_bytes && s[bytes] != '\0') {
        unsigned char c = (unsigned char)s[bytes];
        if ((c & 0xC0) == 0x80) { /* continuation byte — skip */
            bytes++;
            continue;
        }
        int c_bytes = utf8_char_len(s + bytes);
        int c_cols = utf8_char_col_width(s + bytes);
        if (c_bytes <= 0) c_bytes = 1;
        if (cols + c_cols > max_cols) break;
        bytes += c_bytes;
        cols += c_cols;
    }
    return bytes;
}

static void putstr(int y, int x, const char *s, int maxn, int attr) {
    if (maxn <= 0 || !s) return;
    int db = bytes_for_cols(s, (int)strlen(s), maxn);
    if (db > 0) {
        attron(attr);
        mvaddnstr(y, x, s, db);
        attroff(attr);
    }
}

static void draw_box(int h, int w, int bh, int bw, int attr,
                     const char **lines, int nlines) {
    int y0 = (h - bh) / 2;
    int x0 = (w - bw) / 2;
    if (y0 < 0) y0 = 0;
    if (x0 < 0) x0 = 0;
    for (int r = 0; r < bh; r++)
        fill(y0 + r, x0, bw, attr);
    for (int i = 0; i < nlines && i < bh - 2; i++)
        putstr(y0 + 1 + i, x0 + 2, lines[i], bw - 4, attr);
}

/* ---- Shortcut layout for 3-row topbar ---- */
typedef struct { const char *key; int label_id; } Shortcut;

static const Shortcut row1_sc[] = {
    {"F3", S_LBL_COPY}, {"F4", S_LBL_PASTE}, {"^Z", S_LBL_UNDO},
    {"^S", S_LBL_SAVE}, {"^K", S_LBL_LINECLR}, {"^Q", S_LBL_EXIT},
    {NULL, 0}
};
static const Shortcut row2_sc[] = {
    {"^L", S_LBL_MOVE}, {"F5", S_LBL_SELECT}, {"F6", S_LBL_ENDSEL},
    {"M-T", S_LBL_SEARCH}, {"M-C", S_LBL_COLOR},
    {NULL, 0}
};
static const Shortcut row3_sc[] = {
    {"M-S", S_LBL_SETTING}, {"^E", S_LBL_ERRORS},
    {"M-H", S_LBL_HELP}, {"Del", S_LBL_DELETE},
    {NULL, 0}
};

static void draw_sc_row(int y, int w, const Shortcut *list, int attr) {
    fill(y, 0, w, attr);
    int x = 1;
    for (int i = 0; list[i].key && x < w - 6; i++) {
        putstr(y, x, "[", w - x, attr);
        x += 1;

        int key_attr = attr | A_BOLD;
        putstr(y, x, list[i].key, w - x, key_attr);
        x += (int)strlen(list[i].key);

        putstr(y, x, "] ", w - x, attr);
        x += 2;

        const char *lbl = tr(list[i].label_id);
        putstr(y, x, lbl, w - x, attr);
        int lbl_cols = 0, p = 0, llen = (int)strlen(lbl);
        while (p < llen) {
            lbl_cols += utf8_char_col_width(lbl + p);
            p += utf8_char_len(lbl + p);
        }
        x += lbl_cols;

        putstr(y, x, "  ", w - x, attr);
        x += 2;
    }
}

/* ==== top bar ==== */
static void draw_topbar(App *app, int w) {
    (void)app;
    int a = COLOR_PAIR(CP_TOPBAR);
    fill(0, 0, w, a);
    attron(a | A_BOLD);
    mvhline(0, 0, '=', w);
    attroff(a | A_BOLD);

    const char *title = tr(S_TITLE_BAR);
    int tl = 0, p = 0, tlen = (int)strlen(title);
    while (p < tlen) {
        tl += utf8_char_col_width(title + p);
        p += utf8_char_len(title + p);
    }
    int tx = (w - tl) / 2;
    if (tx < 0) tx = 0;
    putstr(0, tx, title, w - tx, a | A_BOLD);

    draw_sc_row(1, w, row1_sc, a);
    draw_sc_row(2, w, row2_sc, a);
    draw_sc_row(3, w, row3_sc, a);

    fill(4, 0, w, a);
    attron(a);
    mvhline(4, 0, '=', w);
    attroff(a);
}

/* ==== explorer panel ==== */
static void draw_explorer(App *app, int y0, int x0, int ht, int wd) {
    int a = COLOR_PAIR(CP_EXPLORER);
    Explorer *ex = &app->ex;
    explorer_flatten(ex);
    if (ex->selected < ex->scroll) ex->scroll = ex->selected;
    if (ex->selected >= ex->scroll + ht) ex->scroll = ex->selected - ht + 1;

    for (int row = 0; row < ht; row++) {
        int idx = ex->scroll + row;
        int y = y0 + row;
        fill(y, x0, wd, a);
        if (idx >= ex->fcount) continue;
        ExpNode *n = ex->flat[idx];
        char mark = n->is_dir ? (n->expanded ? 'v' : '>') : ' ';
        char label[256];
        snprintf(label, sizeof(label), "%*s%c %s", n->depth * 2, "", mark, n->name);
        int ua = (idx == ex->selected && app->focus == FOCUS_EXPLORER)
                 ? (int)COLOR_PAIR(CP_EXPLORER_SEL) : a;
        putstr(y, x0, label, wd - 1, ua);
    }
    /* vertical separator */
    int sa = COLOR_PAIR(CP_TOPBAR);
    attron(sa);
    for (int r = 0; r < ht; r++)
        mvaddch(y0 + r, x0 + wd, ACS_VLINE);
    attroff(sa);
}

/* ==== syntax highlighter ==== */
static const char *kw_control[] = {
    "if","else","for","while","do","return","switch","case","break",
    "continue","goto","default","sizeof","try","except","catch","throw",
    "import","from","export","async","await","def","function","var","let",
    "in","is","not","and","or","yield","with","as","pass","raise",
    "elif","lambda","del","finally","new","delete","typeof","instanceof",
    "extends","implements","interface","package","private","protected","public",
    "abstract","final","override","virtual",
    NULL
};

static const char *kw_types[] = {
    "void","int","char","float","double","long","short","unsigned","signed",
    "size_t","uint8_t","uint16_t","uint32_t","uint64_t","int8_t","int16_t","int32_t","int64_t",
    "bool","boolean","const","static","volatile","extern","inline","register","restrict",
    "struct","enum","union","typedef","class","auto",
    "string","number","object","undefined","symbol","bigint",
    NULL
};

static const char *kw_constants[] = {
    "NULL","nullptr","true","false","True","False","None","nil",
    "this","self","super","__init__","__name__","__main__",
    "NaN","Infinity","undefined",
    NULL
};

static const char *preproc_directives[] = {
    "include","define","undef","ifdef","ifndef","endif","elif","else",
    "if","error","warning","pragma","line",
    NULL
};

static int is_in_list(const char *tok, int len, const char *list[]) {
    for (int i = 0; list[i]; i++) {
        if ((int)strlen(list[i]) == len && strncmp(tok, list[i], len) == 0)
            return 1;
    }
    return 0;
}

static int is_all_caps(const char *tok, int len) {
    if (len < 2) return 0;
    for (int i = 0; i < len; i++) {
        if (!isupper((unsigned char)tok[i]) && tok[i] != '_' && !isdigit((unsigned char)tok[i]))
            return 0;
    }
    return 1;
}

static void draw_syntax_chunk(const char *text, int start, int end, int attr, int *rem_cols) {
    if (*rem_cols <= 0 || start >= end) return;
    int max_bytes = end - start;
    int db = 0;
    int cols = 0;
    int b = 0;
    while (b < max_bytes && text[start + b] != '\0') {
        int c_cols = utf8_char_col_width(text + start + b);
        int c_bytes = utf8_char_len(text + start + b);
        if (c_bytes <= 0) c_bytes = 1;
        if (cols + c_cols > *rem_cols) break;
        b += c_bytes;
        cols += c_cols;
        db = b;
    }
    if (db > 0) {
        char safe_buf[LINE_BUF];
        int print_bytes = db < (int)sizeof(safe_buf) ? db : (int)sizeof(safe_buf) - 1;
        for (int i = 0; i < print_bytes; i++) {
            unsigned char c = (unsigned char)text[start + i];
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                safe_buf[i] = '?';
            } else {
                safe_buf[i] = text[start + i];
            }
        }
        attron(attr);
        addnstr(safe_buf, print_bytes);
        attroff(attr);
    }
    *rem_cols -= cols;
}

static int is_preproc_line(const char *trimmed) {
    if (*trimmed != '#') return 0;
    trimmed++;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
    /* Check if followed by a known preprocessor directive or nothing */
    if (!*trimmed) return 1;
    for (int i = 0; preproc_directives[i]; i++) {
        int dlen = (int)strlen(preproc_directives[i]);
        if (strncmp(trimmed, preproc_directives[i], dlen) == 0 &&
            !isalnum((unsigned char)trimmed[dlen]) && trimmed[dlen] != '_')
            return 1;
    }
    return 0;
}

static void draw_syntax_line(int y, int x, const char *text, int max_w, int base_attr) {
    int len = (int)strlen(text);
    if (max_w <= 0 || len == 0) return;

    /* Find first non-whitespace */
    const char *t = text;
    while (*t == ' ' || *t == '\t') t++;

    /* Full-line preprocessor directive: #include, #define, etc. */
    if (is_preproc_line(t)) {
        putstr(y, x, text, max_w, COLOR_PAIR(CP_SYNTAX_PREPROC) | A_BOLD);
        return;
    }

    /* Full-line comment detection */
    if (strncmp(t, "//", 2) == 0
        || strncmp(t, "/*", 2) == 0 || strncmp(t, "<!--", 4) == 0
        || strncmp(t, " *", 2) == 0 || *t == '*') {
        putstr(y, x, text, max_w, COLOR_PAIR(CP_SYNTAX_COMMENT));
        return;
    }

    /* Shell/Python style comment: # at start of line (NOT C preprocessor) */
    if (*t == '#' && !is_preproc_line(t)) {
        putstr(y, x, text, max_w, COLOR_PAIR(CP_SYNTAX_COMMENT));
        return;
    }

    move(y, x);
    int pos = 0;
    int rem_cols = max_w;

    while (pos < len && rem_cols > 0) {
        char c = text[pos];

        /* Inline comment: // or slash-star */
        if (c == '/' && pos + 1 < len && (text[pos+1] == '/' || text[pos+1] == '*')) {
            draw_syntax_chunk(text, pos, len, COLOR_PAIR(CP_SYNTAX_COMMENT), &rem_cols);
            break;
        }

        /* Inline # comment (Python/Shell style, only if not at line start) */
        if (c == '#' && pos > 0) {
            /* Quick heuristic: if preceded by space, treat as comment */
            if (text[pos-1] == ' ' || text[pos-1] == '\t') {
                draw_syntax_chunk(text, pos, len, COLOR_PAIR(CP_SYNTAX_COMMENT), &rem_cols);
                break;
            }
        }

        /* String literal */
        if (c == '"' || c == '\'') {
            char q = c;
            int start = pos++;
            while (pos < len && text[pos] != q) {
                if (text[pos] == '\\' && pos + 1 < len) pos++;
                pos++;
            }
            if (pos < len) pos++;
            draw_syntax_chunk(text, start, pos, COLOR_PAIR(CP_SYNTAX_STRING), &rem_cols);
            continue;
        }

        /* Backtick template literal (JavaScript) */
        if (c == '`') {
            int start = pos++;
            while (pos < len && text[pos] != '`') {
                if (text[pos] == '\\' && pos + 1 < len) pos++;
                pos++;
            }
            if (pos < len) pos++;
            draw_syntax_chunk(text, start, pos, COLOR_PAIR(CP_SYNTAX_STRING), &rem_cols);
            continue;
        }

        /* Number */
        if (isdigit((unsigned char)c) &&
            (pos == 0 || !isalpha((unsigned char)text[pos-1]))) {
            int start = pos++;
            while (pos < len && (isalnum((unsigned char)text[pos])
                   || text[pos] == '.' || text[pos] == 'x' || text[pos] == 'X'))
                pos++;
            draw_syntax_chunk(text, start, pos, COLOR_PAIR(CP_SYNTAX_NUMBER) | A_BOLD, &rem_cols);
            continue;
        }

        /* Identifier / keyword / function / type / macro / variable / constant */
        if (isalpha((unsigned char)c) || c == '_') {
            int start = pos++;
            while (pos < len && (isalnum((unsigned char)text[pos]) || text[pos] == '_'))
                pos++;
            int w = pos - start;
            int attr = COLOR_PAIR(CP_SYNTAX_VARIABLE);

            if (is_in_list(text + start, w, kw_control)) {
                attr = COLOR_PAIR(CP_SYNTAX_KEYWORD) | A_BOLD;
            } else if (is_in_list(text + start, w, kw_types)) {
                attr = COLOR_PAIR(CP_SYNTAX_TYPE) | A_BOLD;
            } else if (is_in_list(text + start, w, kw_constants)) {
                attr = COLOR_PAIR(CP_SYNTAX_MACRO) | A_BOLD;
            } else if (isupper((unsigned char)text[start]) && !islower((unsigned char)text[start])) {
                /* Starts with uppercase */
                if (is_all_caps(text + start, w)) {
                    attr = COLOR_PAIR(CP_SYNTAX_MACRO) | A_BOLD;
                } else {
                    attr = COLOR_PAIR(CP_SYNTAX_TYPE) | A_BOLD;
                }
            } else {
                /* Check if followed by '(' -> function call */
                int pnext = pos;
                while (pnext < len && (text[pnext] == ' ' || text[pnext] == '\t')) pnext++;
                if (pnext < len && text[pnext] == '(') {
                    attr = COLOR_PAIR(CP_SYNTAX_FUNCTION) | A_BOLD;
                }
            }

            draw_syntax_chunk(text, start, pos, attr, &rem_cols);
            continue;
        }

        /* Operator / Punctuation / Brackets */
        {
            int start = pos++;
            if (strchr("->.:;=+-*/%&|^!?,(){}[]<>", c)) {
                while (pos < len && strchr("->.:;=+-*/%&|^!?,(){}[]<>", text[pos])) pos++;
                draw_syntax_chunk(text, start, pos, COLOR_PAIR(CP_SYNTAX_OPERATOR) | A_BOLD, &rem_cols);
                continue;
            }

            while (pos < len) {
                char nc = text[pos];
                if (nc == '"' || nc == '\'' || nc == '/' || nc == '`' || nc == '#') break;
                if (isdigit((unsigned char)nc) && !isalpha((unsigned char)text[pos-1])) break;
                if (isalpha((unsigned char)nc) || nc == '_') break;
                if (strchr("->.:;=+-*/%&|^!?,(){}[]<>", nc)) break;
                pos++;
            }
            draw_syntax_chunk(text, start, pos, base_attr, &rem_cols);
        }
    }
}

/* ==== editor panel ==== */
static void draw_editor(App *app, int y0, int x0, int ht, int wd) {
    int a = COLOR_PAIR(CP_EDITOR);
    int la = COLOR_PAIR(CP_LINENO);
    Editor *ed = &app->ed;

    /* Scroll to keep cursor visible */
    if (ed->cy < ed->top) ed->top = ed->cy;
    if (ed->cy >= ed->top + ht) ed->top = ed->cy - ht + 1;

    int sel_lo = -1, sel_hi = -1;
    editor_selection(ed, &sel_lo, &sel_hi);
    int tx = x0 + LINENO_W;
    int tw = wd - LINENO_W;
    if (tw < 1) tw = 1;

    for (int row = 0; row < ht; row++) {
        int ln = ed->top + row;
        int y = y0 + row;
        int is_cur = (ln == ed->cy && app->focus == FOCUS_EDITOR);

        /* Always fill with default editor background (black) */
        fill(y, x0, wd, a);
        if (ln >= ed->nlines) continue;

        /* Line number:
         *   Current line → bold yellow number (stands out clearly)
         *   Other lines  → dim cyan number */
        char lnbuf[16];
        snprintf(lnbuf, sizeof(lnbuf), "%4d ", ln + 1);
        if (is_cur) {
            putstr(y, x0, lnbuf, LINENO_W,
                   COLOR_PAIR(CP_CURLINE) | A_BOLD);
        } else {
            putstr(y, x0, lnbuf, LINENO_W, la);
        }

        /* Line content:
         *   Selected lines → inverted (white on black)
         *   All other lines (including current) → syntax highlighted
         *   The cursor itself is shown by ncurses move() below */
        if (sel_lo >= 0 && ln >= sel_lo && ln <= sel_hi) {
            putstr(y, tx, ed->lines[ln].text, tw,
                   COLOR_PAIR(CP_SELECTION));
        } else {
            draw_syntax_line(y, tx, ed->lines[ln].text, tw, a);
        }
    }

    /* Hint when no file is loaded */
    if (!ed->filepath)
        putstr(y0, tx, tr(S_NO_FILE_HINT), tw, a | A_DIM);

    /* Cursor: highlight the character at cursor position using our own text
     * data (NOT mvinch which garbles multibyte characters). */
    if (app->focus == FOCUS_EDITOR) {
        int cr = y0 + (ed->cy - ed->top);
        const char *txt = (ed->cy < ed->nlines) ? ed->lines[ed->cy].text : "";
        int text_len = (int)strlen(txt);
        int pos = 0;
        int col_offset = 0;
        int ed_cx_aligned = utf8_align_char(txt, ed->cx);

        /* Walk through text to find display column of cursor byte position */
        while (pos < ed_cx_aligned && pos < text_len) {
            int c_cols = utf8_char_col_width(txt + pos);
            int c_bytes = utf8_char_len(txt + pos);
            pos += c_bytes;
            col_offset += c_cols;
        }

        int cc = tx + col_offset;
        if (cc >= x0 + wd) cc = x0 + wd - 1;

        /* Draw visible cursor highlight from our text buffer */
        int cur_attr = A_REVERSE | A_BOLD | COLOR_PAIR(CP_EDITOR);
        if (pos < text_len) {
            int mb = utf8_char_len(txt + pos);
            attron(cur_attr);
            mvaddnstr(cr, cc, txt + pos, mb);
            attroff(cur_attr);
        } else {
            /* Cursor is at end of line: draw a highlighted space */
            mvaddch(cr, cc, ' ' | cur_attr);
        }

        move(cr, cc);
        curs_set(1);
    } else {
        curs_set(0);
    }
}

/* ==== status bar ==== */
static void draw_status(App *app, int y, int w) {
    int a = COLOR_PAIR(CP_ACCENT);
    Editor *ed = &app->ed;
    const char *mod = ed->modified ? "*" : "";
    const char *fn = ed->filepath ? ed->filepath : "(no file)";
    const char *foc = app->focus == FOCUS_EDITOR
                      ? tr(S_WORD_EDITOR) : tr(S_WORD_EXPLORER);
    char left[1024];
    snprintf(left, sizeof(left), " %s%s  [%s]  %s",
             mod, fn, foc, app->status);
    char right[64];
    snprintf(right, sizeof(right), "Ln %d, Col %d ",
             ed->cy + 1, ed->cx + 1);
    attron(a);
    mvhline(y, 0, ' ', w);
    mvaddnstr(y, 0, left, w - (int)strlen(right) - 1);
    mvaddnstr(y, w - (int)strlen(right), right, (int)strlen(right));
    attroff(a);
}

/* ==== overlays ==== */
#define NUM_SC_HELP 15
static const char *sc_keys_help[] = {
    "^C","^S","^K","^V","^Z","^Q",
    "M-C","^L","M-T","F5","F6",
    "M-S","^E","M-H","Del"
};
static const int sc_lbl_help[] = {
    S_LBL_COPY, S_LBL_SAVE, S_LBL_LINECLR, S_LBL_PASTE,
    S_LBL_UNDO, S_LBL_EXIT,
    S_LBL_COLOR, S_LBL_MOVE, S_LBL_SEARCH, S_LBL_SELECT,
    S_LBL_ENDSEL,
    S_LBL_SETTING, S_LBL_ERRORS, S_LBL_HELP, S_LBL_DELETE
};

static void draw_help(App *app, int h, int w) {
    (void)app;
    const char *content[32];
    int cl = 0;
    content[cl++] = tr(S_OVL_HELP_T);
    content[cl++] = "";
    static char hl[NUM_SC_HELP][64];
    for (int i = 0; i < NUM_SC_HELP; i++) {
        snprintf(hl[i], sizeof(hl[i]), "%5s  %s",
                 sc_keys_help[i], tr(sc_lbl_help[i]));
        content[cl++] = hl[i];
    }
    content[cl++] = "";
    content[cl++] = tr(S_OVL_ANYKEY);
    int bh = cl + 2;
    int bw = 40;
    if (bw > w - 4) bw = w - 4;
    draw_box(h, w, bh, bw, COLOR_PAIR(CP_OVERLAY), content, cl);
}

static void draw_errors(App *app, int h, int w) {
    const char *content[MAX_ERRORS + 6];
    int cl = 0;
    content[cl++] = tr(S_OVL_ERR_T);
    content[cl++] = "";
    if (!app->nerr) {
        content[cl++] = tr(S_NO_ERRORS);
    } else {
        int s = app->nerr > 10 ? app->nerr - 10 : 0;
        for (int i = s; i < app->nerr; i++)
            content[cl++] = app->errors[i];
    }
    content[cl++] = "";
    content[cl++] = tr(S_OVL_ANYKEY);
    int bh = cl + 2;
    if (bh > h - 4) bh = h - 4;
    int bw = 50;
    if (bw > w - 4) bw = w - 4;
    draw_box(h, w, bh, bw, COLOR_PAIR(CP_OVERLAY), content, cl);
}

static void draw_settings(App *app, int h, int w) {
    static char code_ln[128], ui_ln[128], lg_ln[128], optl[4][128];
    snprintf(code_ln, sizeof(code_ln), tr(S_OVL_CUR_CODE_COLOR),
             g_schemes[app->cfg.code_scheme].name);
    snprintf(ui_ln, sizeof(ui_ln), tr(S_OVL_CUR_UI_COLOR),
             g_schemes[app->cfg.ui_scheme].name);
    snprintf(lg_ln, sizeof(lg_ln), tr(S_OVL_CUR_LANG),
             app->cfg.language == LANG_JP ? tr(S_OVL_JP) : tr(S_OVL_EN));
    const char *opts[] = {
        tr(S_OVL_CHG_CODE_COLOR), tr(S_OVL_CHG_UI_COLOR),
        tr(S_OVL_CHG_LANG), tr(S_OVL_CLOSE)
    };
    const char *content[16];
    int cl = 0;
    content[cl++] = tr(S_OVL_SET_T);
    content[cl++] = "";
    for (int i = 0; i < 4; i++) {
        snprintf(optl[i], sizeof(optl[i]), "%s %s",
                 i == app->settings_cur ? ">" : " ", opts[i]);
        content[cl++] = optl[i];
    }
    content[cl++] = "";
    content[cl++] = code_ln;
    content[cl++] = ui_ln;
    content[cl++] = lg_ln;
    content[cl++] = tr(S_OVL_ARROWS);
    int bh = cl + 2;
    if (bh > h - 4) bh = h - 4;
    int bw = 48;
    if (bw > w - 4) bw = w - 4;
    draw_box(h, w, bh, bw, COLOR_PAIR(CP_OVERLAY), content, cl);
}

static void draw_confirm(App *app, int h, int w) {
    static char msg[384];
    snprintf(msg, sizeof(msg), tr(S_MSG_CONFIRM), app->confirm_name);
    const char *content[2];
    content[0] = msg;
    int bw = (int)strlen(msg) + 8;
    if (bw > w - 4) bw = w - 4;
    if (bw < 30) bw = 30;
    draw_box(h, w, 3, bw, COLOR_PAIR(CP_OVERLAY) | A_BOLD, content, 1);
}

static void draw_input_modal(App *app, int h, int w, int is_file) {
    static char title_line[128];
    static char dir_line[MAXPATH + 16];
    static char input_line[300];

    snprintf(title_line, sizeof(title_line), "%s",
             is_file ? tr(S_OVL_NEWFILE_T) : tr(S_OVL_NEWDIR_T));
    snprintf(dir_line, sizeof(dir_line), "Location: %.4085s", app->target_dir);
    snprintf(input_line, sizeof(input_line), "%s %s_",
             tr(S_OVL_PROMPT_NAME), app->input_buf);

    const char *content[6];
    int cl = 0;
    content[cl++] = title_line;
    content[cl++] = "";
    content[cl++] = dir_line;
    content[cl++] = input_line;
    content[cl++] = "";
    content[cl++] = tr(S_OVL_INPUT_HINT);

    int bh = cl + 2;
    int bw = 54;
    if (bw > w - 4) bw = w - 4;
    draw_box(h, w, bh, bw, COLOR_PAIR(CP_OVERLAY) | A_BOLD, content, cl);
}

static void draw_convert_popup(App *app, int h, int w) {
    ConvertState *conv = &app->conv;
    if (conv->convert_ncand <= 0) return;

    int ew = 0;
    if (!app->is_file_mode) {
        ew = w / EXPLORER_DIV;
        if (ew < EXPLORER_MIN_W) ew = EXPLORER_MIN_W;
        if (ew > w / 2) ew = w / 2;
    }
    int ed_x0 = (ew > 0) ? (ew + 1) : 0;
    Editor *ed = &app->ed;
    int cr = TOPBAR_H + (ed->cy - ed->top);
    const char *txt = (ed->cy < ed->nlines) ? ed->lines[ed->cy].text : "";
    int text_len = (int)strlen(txt);
    int pos = 0, col_offset = 0;
    int ed_cx_aligned = utf8_align_char(txt, conv->convert_start_cx);
    while (pos < ed_cx_aligned && pos < text_len) {
        int c_cols = utf8_char_col_width(txt + pos);
        int c_bytes = utf8_char_len(txt + pos);
        pos += c_bytes;
        col_offset += c_cols;
    }
    int tx = ed_x0 + LINENO_W;
    int cc = tx + col_offset;

    /* Calculate popup box dimensions */
    int max_item_w = 16;
    for (int i = 0; i < conv->convert_ncand; i++) {
        int item_cols = 0;
        int p = 0;
        int ilen = (int)strlen(conv->convert_cands[i]);
        while (p < ilen) {
            item_cols += utf8_char_col_width(conv->convert_cands[i] + p);
            p += utf8_char_len(conv->convert_cands[i] + p);
        }
        if (item_cols + 6 > max_item_w) max_item_w = item_cols + 6;
    }
    if (max_item_w > 40) max_item_w = 40;

    int bh = conv->convert_ncand + 2;
    if (bh > 10) bh = 10;
    int bw = max_item_w;

    /* Position box right below cursor, or above if close to bottom */
    int box_y = cr + 1;
    if (box_y + bh >= h - 1) box_y = cr - bh;
    if (box_y < TOPBAR_H) box_y = TOPBAR_H;

    int box_x = cc;
    if (box_x + bw >= w) box_x = w - bw - 1;
    if (box_x < ed_x0) box_x = ed_x0;

    /* Draw box background */
    int attr_box = COLOR_PAIR(CP_OVERLAY) | A_BOLD;
    int attr_sel = COLOR_PAIR(CP_EXPLORER_SEL) | A_BOLD;

    fill(box_y, box_x, bw, attr_box);
    putstr(box_y, box_x + 1, " 変換 (Space/Enter)", bw - 2, attr_box);

    for (int i = 0; i < conv->convert_ncand && i < bh - 2; i++) {
        int py = box_y + 1 + i;
        int is_sel = (i == conv->convert_sel);
        int item_attr = is_sel ? attr_sel : COLOR_PAIR(CP_OVERLAY);
        fill(py, box_x, bw, item_attr);

        char linebuf[256];
        snprintf(linebuf, sizeof(linebuf), " %d. %s", i + 1, conv->convert_cands[i]);
        putstr(py, box_x + 1, linebuf, bw - 2, item_attr);
    }
    fill(box_y + bh - 1, box_x, bw, attr_box);
}

/* ==== main composite draw ==== */
void ui_draw(App *app) {
    erase();
    int h, w;
    getmaxyx(stdscr, h, w);

    draw_topbar(app, w);

    int ew = 0;
    if (!app->is_file_mode) {
        ew = w / EXPLORER_DIV;
        if (ew < EXPLORER_MIN_W) ew = EXPLORER_MIN_W;
        if (ew > w / 2) ew = w / 2;
    }
    int bh = h - TOPBAR_H - 1;
    if (bh < 1) bh = 1;

    if (ew > 0) {
        draw_explorer(app, TOPBAR_H, 0, bh, ew);
        draw_editor(app, TOPBAR_H, ew + 1, bh, w - ew - 1);
    } else {
        draw_editor(app, TOPBAR_H, 0, bh, w);
    }
    draw_status(app, h - 1, w);

    switch (app->overlay) {
        case OVL_HELP:     draw_help(app, h, w);        break;
        case OVL_ERRORS:   draw_errors(app, h, w);      break;
        case OVL_SETTINGS: draw_settings(app, h, w);    break;
        case OVL_CONFIRM:  draw_confirm(app, h, w);     break;
        case OVL_NEW_FILE: draw_input_modal(app, h, w, 1); break;
        case OVL_NEW_DIR:  draw_input_modal(app, h, w, 0); break;
        case OVL_CONVERT:  draw_convert_popup(app, h, w); break;
    }

    /* Move physical terminal cursor to active input location so IME / Terminal cursor
     * appears at the exact typing location instead of top-left (0,0) or bottom status bar. */
    if (app->overlay == OVL_NEW_FILE || app->overlay == OVL_NEW_DIR) {
        int cl = 6;
        int mbh = cl + 2;
        int y0 = (h - mbh) / 2;
        int x0 = (w - 54) / 2;
        if (x0 < 0) x0 = 0;
        int prompt_len = (int)strlen(tr(S_OVL_PROMPT_NAME)) + 1;
        int input_cols = 0;
        int pos = 0;
        int len = (int)strlen(app->input_buf);
        while (pos < app->input_pos && pos < len) {
            int c_cols = utf8_char_col_width(app->input_buf + pos);
            int c_bytes = utf8_char_len(app->input_buf + pos);
            pos += c_bytes;
            input_cols += c_cols;
        }
        move(y0 + 4, x0 + 2 + prompt_len + input_cols);
        curs_set(1);
    } else if (app->overlay == OVL_SETTINGS) {
        /* No text entry happens in the settings menu — hide the caret.
         * (The old iTerm2-only OSC 1337 SetInputMethod sequence did
         * nothing on Windows Terminal/ConPTY, so it's gone; curs_set(0)
         * plus the flushinp() guards in handle_settings() are what
         * actually keep IME output from leaking into app state.) */
        curs_set(0);
    } else if (app->overlay == OVL_CONVERT || app->focus == FOCUS_EDITOR) {
        Editor *ed = &app->ed;
        int cr = TOPBAR_H + (ed->cy - ed->top);
        const char *txt = (ed->cy < ed->nlines) ? ed->lines[ed->cy].text : "";
        int text_len = (int)strlen(txt);
        int pos = 0;
        int col_offset = 0;
        int ed_cx_aligned = utf8_align_char(txt, ed->cx);
        while (pos < ed_cx_aligned && pos < text_len) {
            int c_cols = utf8_char_col_width(txt + pos);
            int c_bytes = utf8_char_len(txt + pos);
            pos += c_bytes;
            col_offset += c_cols;
        }
        int ed_x0 = (ew > 0) ? (ew + 1) : 0;
        int tx = ed_x0 + LINENO_W;
        int cc = tx + col_offset;
        if (cc >= w) cc = w - 1;
        if (cr < TOPBAR_H) cr = TOPBAR_H;
        if (cr >= h - 1) cr = h - 2;
        move(cr, cc);
        curs_set(1);
    } else if (app->overlay != OVL_NONE) {
        move(h / 2, w / 2);
        curs_set(0);
    } else if (app->focus == FOCUS_EXPLORER) {
        Explorer *ex = &app->ex;
        int cr = TOPBAR_H + (ex->selected - ex->scroll);
        if (cr < TOPBAR_H) cr = TOPBAR_H;
        if (cr >= h - 1) cr = h - 2;
        move(cr, 2);
        /* Explorer never accepts text input (selection is shown via
         * highlight, not a caret) — keep the cursor hidden so terminals
         * that trigger IME on a visible caret have nothing to latch onto. */
        curs_set(0);
    }

    refresh();
}
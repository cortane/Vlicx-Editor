/* main.c — entry point, input handling, main loop */
#include "vlicx.h"

static struct termios g_orig;
static int g_saved = 0;

/* ---- error log ---- */
void app_error(App *app, const char *msg) {
    if (app->nerr >= MAX_ERRORS) {
        free(app->errors[0]);
        memmove(&app->errors[0], &app->errors[1], sizeof(char *) * (MAX_ERRORS - 1));
        app->nerr--;
    }
    app->errors[app->nerr++] = strdup(msg);
}

/* ---- input ---- */
#define INP_KEY 0
#define INP_ALT 1
typedef struct { int kind, key; } Input;

static Input get_input(void) {
    Input r = {INP_KEY, 0};
    int ch = getch();
    if (ch == 27) {
        nodelay(stdscr, TRUE);
        int nxt = getch();
        nodelay(stdscr, FALSE);
        if (nxt == ERR) r.key = 27;
        else { r.kind = INP_ALT; r.key = nxt; }
    } else r.key = ch;
    return r;
}

/* ---- clipboard ---- */
static void do_copy(App *a) {
    Editor *e = &a->ed;
    free(e->clipboard); e->clipboard = NULL;
    if (e->sel_start >= 0) {
        e->clipboard = editor_selected_text(e);
        snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_COPY_SEL));
        e->sel_start = -1;
        e->sel_active = 0;
    } else {
        e->clipboard = strdup(e->lines[e->cy].text);
        snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_COPY_LINE));
    }
}
static void do_paste(App *a) {
    Editor *e = &a->ed;
    if (!e->clipboard || !*e->clipboard) return;
    for (const char *p = e->clipboard; *p; p++) {
        if (*p == '\n') editor_newline(e);
        else editor_insert_char(e, (unsigned char)*p);
    }
    snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_PASTED));
}

/* ---- search ---- */
static void do_search(App *a) {
    Editor *e = &a->ed;
    char *text = NULL;
    if (e->sel_start >= 0) {
        text = editor_selected_text(e);
    } else {
        const char *ln = e->lines[e->cy].text;
        while (*ln == ' ' || *ln == '\t') ln++;
        if (*ln) text = strdup(ln);
    }
    if (!text || !*text) {
        snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_NO_SEARCH));
        free(text); return;
    }
    int len = (int)strlen(text);
    while (len > 0 && (text[len-1]==' '||text[len-1]=='\t'||text[len-1]=='\n'))
        text[--len] = '\0';
    int found = editor_search_wrap(e, text);
    if (found) snprintf(a->status, MAX_STATUS, tr(S_MSG_FOUND), found);
    else       snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_NOT_FOUND));
    free(text);
}

/* ---- selection clear helper ---- */
static void clear_sel(App *a) {
    Editor *e = &a->ed;
    if (e->sel_start >= 0 && !e->sel_active) e->sel_start = -1;
}

/* ---- key handlers ---- */
static void handle_alt(App *a, int ch) {
    int c = tolower(ch);
    if (c == 'c' || c == 's') {
        a->overlay = OVL_SETTINGS;
        a->settings_cur = (c == 'c') ? 0 : 1;
        curs_set(0);
        flushinp();
    }
    else if (c == 'v') do_paste(a);
    else if (c == 't') do_search(a);
    else if (c == 'h') a->overlay = OVL_HELP;
}

static void handle_explorer_key(App *a, int key) {
    Explorer *ex = &a->ex;
    if (key == KEY_UP)   explorer_move(ex, -1);
    else if (key == KEY_DOWN) explorer_move(ex, 1);
    else if (key == KEY_RIGHT || key == 10 || key == 13 || key == KEY_ENTER) {
        ExpNode *n = explorer_current(ex);
        if (!n) return;
        if (n->is_dir) {
            if ((key == 10 || key == 13 || key == KEY_ENTER) && n->expanded)
                explorer_collapse(ex);
            else explorer_expand(ex);
        } else {
            editor_load(&a->ed, n->path);
            a->focus = FOCUS_EDITOR;
            snprintf(a->status, MAX_STATUS, tr(S_MSG_OPENED), n->name);
        }
    } else if (key == KEY_LEFT) explorer_collapse(ex);
    else {
        /* Reject anything that isn't an arrow key or Enter/Right (expand).
         * This also drains any further bytes of an IME-committed UTF-8
         * sequence so a Japanese string can't leak in byte-by-byte. */
        flushinp();
    }
}

static int start_conversion(App *a) {
    char query[128];
    int start_cx = 0, end_cx = 0;
    if (editor_find_hiragana_before_cursor(&a->ed, &start_cx, &end_cx, query, sizeof(query))) {
        dict_free_cands(a->conv.convert_cands, a->conv.convert_ncand);
        snprintf(a->conv.convert_query, sizeof(a->conv.convert_query), "%s", query);
        a->conv.convert_start_cx = start_cx;
        a->conv.convert_end_cx = end_cx;
        a->conv.convert_ncand = dict_lookup(query, a->conv.convert_cands, MAX_CONVERT_CANDS);
        if (a->conv.convert_ncand > 0) {
            a->conv.convert_sel = 0;
            a->overlay = OVL_CONVERT;
            return 1;
        }
    }
    return 0;
}

static void handle_editor_key(App *a, int key) {
    Editor *e = &a->ed;
    if      (key == KEY_UP)    editor_move(e, -1, 0);
    else if (key == KEY_DOWN)  editor_move(e, 1, 0);
    else if (key == KEY_LEFT)  editor_move(e, 0, -1);
    else if (key == KEY_RIGHT) editor_move(e, 0, 1);
    else if (key == KEY_HOME)  e->cx = 0;
    else if (key == KEY_END)   e->cx = e->lines[e->cy].len;
    else if (key == KEY_PPAGE) { int h,w; getmaxyx(stdscr,h,w); (void)w;
                                 editor_move(e, -(h-TOPBAR_H-2), 0); }
    else if (key == KEY_NPAGE) { int h,w; getmaxyx(stdscr,h,w); (void)w;
                                 editor_move(e, h-TOPBAR_H-2, 0); }
    else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        editor_backspace(e); clear_sel(a);
    }
    else if (key == KEY_DC) { editor_delete_char(e); clear_sel(a); }
    else if (key == 10 || key == 13 || key == KEY_ENTER) {
        editor_newline(e); clear_sel(a);
    }
    else if (key == '\t') {
        if (!start_conversion(a)) {
            for (int i = 0; i < TAB_STOP; i++) editor_insert_char(e, ' ');
            clear_sel(a);
        }
    }
    else if (key == ' ') {
        if (!start_conversion(a)) {
            editor_insert_char(e, ' ');
            clear_sel(a);
        }
    }
    else if (key >= 32 && key != 127 && key < KEY_MIN) { editor_insert_char(e, key); clear_sel(a); }
}

static void handle_settings(App *a, Input inp) {
    /* Always flush any pending IME / typeahead input first */
    flushinp();
    if (inp.kind != INP_KEY) {
        return;
    }
    int k = inp.key;
    if (k == KEY_UP) {
        a->settings_cur = (a->settings_cur > 0) ? a->settings_cur - 1 : 3;
    } else if (k == KEY_DOWN) {
        a->settings_cur = (a->settings_cur < 3) ? a->settings_cur + 1 : 0;
    } else if (k == KEY_RIGHT || k == 10 || k == 13 || k == KEY_ENTER) {
        if (a->settings_cur == 0) {
            a->cfg.code_scheme = (a->cfg.code_scheme + 1) % g_nschemes;
            ui_init_colors(a->cfg.code_scheme, a->cfg.ui_scheme);
            config_save(&a->cfg);
        } else if (a->settings_cur == 1) {
            a->cfg.ui_scheme = (a->cfg.ui_scheme + 1) % g_nschemes;
            ui_init_colors(a->cfg.code_scheme, a->cfg.ui_scheme);
            config_save(&a->cfg);
        } else if (a->settings_cur == 2) {
            a->cfg.language = (a->cfg.language + 1) % LANG_COUNT;
            i18n_set_language(a->cfg.language);
            config_save(&a->cfg);
        } else {
            a->overlay = OVL_NONE;
            curs_set(1);
            fputs("\033[?25h\033]1337;SetInputMethod=1\007", stdout);
            fflush(stdout);
        }
    } else if (k == KEY_LEFT) {
        if (a->settings_cur == 0) {
            a->cfg.code_scheme = (a->cfg.code_scheme + g_nschemes - 1) % g_nschemes;
            ui_init_colors(a->cfg.code_scheme, a->cfg.ui_scheme);
            config_save(&a->cfg);
        } else if (a->settings_cur == 1) {
            a->cfg.ui_scheme = (a->cfg.ui_scheme + g_nschemes - 1) % g_nschemes;
            ui_init_colors(a->cfg.code_scheme, a->cfg.ui_scheme);
            config_save(&a->cfg);
        } else if (a->settings_cur == 2) {
            a->cfg.language = (a->cfg.language + 1) % LANG_COUNT;
            i18n_set_language(a->cfg.language);
            config_save(&a->cfg);
        }
    } else if (k == 27) {
        a->overlay = OVL_NONE;
        curs_set(1);
        fputs("\033[?25h\033]1337;SetInputMethod=1\007", stdout);
        fflush(stdout);
    } else {
        /* Reject and flush all IME and character typeahead input */
        flushinp();
    }
}

static void handle_confirm(App *a, Input inp) {
    if (inp.kind != INP_KEY) return;
    if (inp.key == 'y' || inp.key == 'Y') {
        char err[256];
        if (explorer_delete_node(&a->ex, err, sizeof(err)) == 0)
            snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_DELETED));
        else {
            snprintf(a->status, MAX_STATUS, tr(S_MSG_DEL_FAIL), err);
            app_error(a, a->status);
        }
    }
    a->overlay = OVL_NONE;
}

static void handle_input_overlay(App *a, Input inp) {
    if (inp.kind != INP_KEY) return;
    int k = inp.key;
    if (k == 27) { /* ESC cancels */
        a->overlay = OVL_NONE;
        return;
    }
    if (k == KEY_BACKSPACE || k == 127 || k == 8) {
        if (a->input_pos > 0) {
            a->input_buf[--a->input_pos] = '\0';
        }
        return;
    }
    if (k == 10 || k == 13 || k == KEY_ENTER) {
        if (a->input_pos == 0) return;
        char full_path[MAXPATH + MAXPATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", a->target_dir, a->input_buf);

        if (a->overlay == OVL_NEW_FILE) {
            FILE *f = fopen(full_path, "w");
            if (f) {
                fclose(f);
                snprintf(a->status, MAX_STATUS, tr(S_MSG_CREATED_FILE), a->input_buf);
                expnode_refresh(a->ex.root);
                explorer_flatten(&a->ex);
                editor_load(&a->ed, full_path);
                a->focus = FOCUS_EDITOR;
            } else {
                snprintf(a->status, MAX_STATUS, tr(S_MSG_CREATE_FAIL), strerror(errno));
                app_error(a, a->status);
            }
        } else if (a->overlay == OVL_NEW_DIR) {
            if (mkdir(full_path, 0755) == 0) {
                snprintf(a->status, MAX_STATUS, tr(S_MSG_CREATED_DIR), a->input_buf);
                expnode_refresh(a->ex.root);
                explorer_flatten(&a->ex);
            } else {
                snprintf(a->status, MAX_STATUS, tr(S_MSG_CREATE_FAIL), strerror(errno));
                app_error(a, a->status);
            }
        }
        a->overlay = OVL_NONE;
        return;
    }
    if (k >= 32 && k < 127 && a->input_pos < (int)sizeof(a->input_buf) - 1) {
        a->input_buf[a->input_pos++] = (char)k;
        a->input_buf[a->input_pos] = '\0';
    }
}

static void handle_key(App *a, int key) {
    if (key == 17) { a->running = 0; return; }              /* ^Q */
    if (key == 19) {                                          /* ^S */
        char msg[MAX_STATUS];
        if (!editor_save(&a->ed, msg, sizeof(msg))) app_error(a, msg);
        snprintf(a->status, MAX_STATUS, "%s", msg); return;
    }
    if (key == 11 && a->focus == FOCUS_EDITOR) {              /* ^K */
        editor_clear_line(&a->ed);
        snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_LINE_CLR)); return;
    }
    if (key == 3  || key == KEY_F(3)) { do_copy(a); return; }   /* ^C or F3 */
    if (key == 22 || key == KEY_F(4)) { do_paste(a); return; }  /* ^V or F4 */
    if (key == 26 || key == KEY_UNDO) {                       /* ^Z */
        if (editor_undo(&a->ed))
            snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_UNDO));
        else
            snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_NO_UNDO));
        return;
    }
    if (key == 12) {                                          /* ^L */
        if (a->is_file_mode) {
            a->is_file_mode = 0;
            a->focus = FOCUS_EXPLORER;
        } else {
            a->focus = (a->focus == FOCUS_EDITOR) ? FOCUS_EXPLORER : FOCUS_EDITOR;
        }
        snprintf(a->status, MAX_STATUS, tr(S_MSG_FOCUS),
                 (a->focus == FOCUS_EDITOR) ? tr(S_WORD_EDITOR) : tr(S_WORD_EXPLORER));
        return;
    }
    if (key == 5) { a->overlay = OVL_ERRORS; return; }       /* ^E */

    if (key == KEY_F(1) && a->focus == FOCUS_EXPLORER) {
        ExpNode *n = explorer_current(&a->ex);
        if (n) {
            if (n->is_dir) snprintf(a->target_dir, sizeof(a->target_dir), "%s", n->path);
            else {
                snprintf(a->target_dir, sizeof(a->target_dir), "%s", n->path);
                char *sl = strrchr(a->target_dir, '/');
                if (sl) *sl = '\0';
            }
            a->input_buf[0] = '\0';
            a->input_pos = 0;
            a->overlay = OVL_NEW_FILE;
        }
        return;
    }
    if (key == KEY_F(2) && a->focus == FOCUS_EXPLORER) {
        ExpNode *n = explorer_current(&a->ex);
        if (n) {
            if (n->is_dir) snprintf(a->target_dir, sizeof(a->target_dir), "%s", n->path);
            else {
                snprintf(a->target_dir, sizeof(a->target_dir), "%s", n->path);
                char *sl = strrchr(a->target_dir, '/');
                if (sl) *sl = '\0';
            }
            a->input_buf[0] = '\0';
            a->input_pos = 0;
            a->overlay = OVL_NEW_DIR;
        }
        return;
    }

    if (key == KEY_F(5) || key == VKEY_CTRL1) {
        if (a->focus == FOCUS_EDITOR) {
            a->ed.sel_start = a->ed.cy; a->ed.sel_active = 1;
            snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_SEL_ON));
        }
        return;
    }
    if (key == KEY_F(6) || key == VKEY_CTRL2 || key == 0) {
        a->ed.sel_active = 0;
        snprintf(a->status, MAX_STATUS, "%s", tr(S_MSG_SEL_OFF));
        return;
    }
    if (key == KEY_DC && a->focus == FOCUS_EXPLORER) {
        ExpNode *n = explorer_current(&a->ex);
        if (n && n != a->ex.root) {
            snprintf(a->confirm_name, sizeof(a->confirm_name), "%s", n->name);
            a->overlay = OVL_CONFIRM;
        }
        return;
    }
    if (key == KEY_RESIZE) return;

    if (a->focus == FOCUS_EXPLORER) handle_explorer_key(a, key);
    else handle_editor_key(a, key);
}

static void handle_convert_overlay(App *a, Input inp) {
    if (inp.kind != INP_KEY) return;
    int k = inp.key;
    ConvertState *c = &a->conv;

    if (k == 27) { /* ESC cancels conversion */
        dict_free_cands(c->convert_cands, c->convert_ncand);
        c->convert_ncand = 0;
        a->overlay = OVL_NONE;
        return;
    }

    if (k == ' ' || k == KEY_DOWN || k == '\t') {
        if (c->convert_ncand > 0) {
            c->convert_sel = (c->convert_sel + 1) % c->convert_ncand;
        }
        return;
    }

    if (k == KEY_UP) {
        if (c->convert_ncand > 0) {
            c->convert_sel = (c->convert_sel + c->convert_ncand - 1) % c->convert_ncand;
        }
        return;
    }

    if (k >= '1' && k <= '9') {
        int idx = k - '1';
        if (idx < c->convert_ncand) {
            c->convert_sel = idx;
            const char *choice = c->convert_cands[c->convert_sel];
            editor_replace_range(&a->ed, c->convert_start_cx, c->convert_end_cx, choice);
            dict_free_cands(c->convert_cands, c->convert_ncand);
            c->convert_ncand = 0;
            a->overlay = OVL_NONE;
            return;
        }
    }

    if (k == 10 || k == 13 || k == KEY_ENTER) {
        if (c->convert_sel >= 0 && c->convert_sel < c->convert_ncand) {
            const char *choice = c->convert_cands[c->convert_sel];
            editor_replace_range(&a->ed, c->convert_start_cx, c->convert_end_cx, choice);
        }
        dict_free_cands(c->convert_cands, c->convert_ncand);
        c->convert_ncand = 0;
        a->overlay = OVL_NONE;
        return;
    }

    /* Any other key confirms current selection and handles that key */
    if (c->convert_sel >= 0 && c->convert_sel < c->convert_ncand) {
        const char *choice = c->convert_cands[c->convert_sel];
        editor_replace_range(&a->ed, c->convert_start_cx, c->convert_end_cx, choice);
    }
    dict_free_cands(c->convert_cands, c->convert_ncand);
    c->convert_ncand = 0;
    a->overlay = OVL_NONE;

    /* Pass through key */
    handle_key(a, k);
}

/* ---- main loop ---- */
static void app_run(App *a) {
    while (a->running) {
        ui_draw(a);
        Input inp = get_input();
        if (a->overlay == OVL_HELP || a->overlay == OVL_ERRORS) {
            a->overlay = OVL_NONE; continue;
        }
        if (a->overlay == OVL_SETTINGS) { handle_settings(a, inp); continue; }
        if (a->overlay == OVL_CONFIRM)  { handle_confirm(a, inp); continue; }
        if (a->overlay == OVL_NEW_FILE || a->overlay == OVL_NEW_DIR) {
            handle_input_overlay(a, inp); continue;
        }
        if (a->overlay == OVL_CONVERT)  { handle_convert_overlay(a, inp); continue; }
        if (inp.kind == INP_ALT) handle_alt(a, inp.key);
        else handle_key(a, inp.key);
    }
}

/* ---- init / cleanup ---- */
static void app_init(App *a, const char *path, int is_file) {
    memset(a, 0, sizeof(*a));
    config_load(&a->cfg);
    i18n_set_language(a->cfg.language);
    editor_init(&a->ed);
    a->is_file_mode = is_file;
    if (is_file) {
        char dir[MAXPATH + MAXPATH];
        snprintf(dir, sizeof(dir), "%s", path);
        char *sl = strrchr(dir, '/');
        if (sl && sl != dir) *sl = '\0';
        else { dir[0] = '.'; dir[1] = '\0'; }
        explorer_init(&a->ex, dir);
        editor_load(&a->ed, path);
        a->focus = FOCUS_EDITOR;
    } else {
        explorer_init(&a->ex, path);
        a->focus = FOCUS_EXPLORER;
    }
    a->running = 1;
}

static void app_cleanup(App *a) {
    editor_free(&a->ed);
    explorer_free(&a->ex);
    for (int i = 0; i < a->nerr; i++) free(a->errors[i]);
    dict_unload();
}

static void term_setup(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig) == 0) {
        g_saved = 1;
        struct termios t = g_orig;
        t.c_lflag &= ~(ISIG);            /* disable signal generation so ^C delivered as key 3 */
        t.c_iflag &= ~(IXON | IXOFF);   /* disable XON/XOFF */
        t.c_cc[VINTR] = _POSIX_VDISABLE;
        t.c_cc[VQUIT] = _POSIX_VDISABLE;
        t.c_cc[VSUSP] = _POSIX_VDISABLE; /* disable Ctrl+Z SIGTSTP so app catches ^Z */
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
}
static void term_restore(void) {
    fputs("\033[0 q", stdout); fflush(stdout);
    if (g_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_orig);
}

static void check_update_notice(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char verpath[1024];
    snprintf(verpath, sizeof(verpath), "%s/.vlicx-version", home);
    FILE *fp = fopen(verpath, "r");
    if (!fp) return;
    char local_sha[128] = {0};
    if (!fgets(local_sha, sizeof(local_sha), fp)) { fclose(fp); return; }
    fclose(fp);

    char *p = local_sha;
    while (*p && (*p == ' ' || *p == '\r' || *p == '\n')) p++;
    char *end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\r' || *end == '\n')) { *end = '\0'; end--; }
    if (!*p) return;

    FILE *pipe = popen("curl -s -m 2 https://api.github.com/repos/cortane/Vlicx-Editor/commits/main 2>/dev/null | grep '\"sha\"' | head -n 1 | cut -d '\"' -f 4", "r");
    if (!pipe) return;
    char remote_sha[128] = {0};
    if (fgets(remote_sha, sizeof(remote_sha), pipe)) {
        char *rp = remote_sha;
        while (*rp && (*rp == ' ' || *rp == '\r' || *rp == '\n')) rp++;
        char *rend = rp + strlen(rp) - 1;
        while (rend > rp && (*rend == ' ' || *rend == '\r' || *rend == '\n')) { *rend = '\0'; rend--; }

        if (*rp && strcmp(p, rp) != 0) {
            fprintf(stderr, "\033[1;33m╭──────────────────────────────────────────────────────────────────────────────╮\033[0m\n");
            fprintf(stderr, "\033[1;33m│ 💡 VlicxEditor に最新バージョンの更新があります！                              │\033[0m\n");
            fprintf(stderr, "\033[1;33m│   更新コマンド:                                                              │\033[0m\n");
            fprintf(stderr, "\033[1;36m│   curl -fsSL https://raw.githubusercontent.com/cortane/Vlicx-Editor/main/install.sh | sh │\033[0m\n");
            fprintf(stderr, "\033[1;33m╰──────────────────────────────────────────────────────────────────────────────╯\033[0m\n");
        }
    }
    pclose(pipe);
}

/* ==== main ==== */
int main(int argc, char *argv[]) {
    if (argc < 4 || strcmp(argv[1], "--mode") != 0) {
        fprintf(stderr, "Usage: vlicx --mode <folder|file> <path>\n");
        return 1;
    }
    int is_file;
    if      (!strcmp(argv[2], "folder")) is_file = 0;
    else if (!strcmp(argv[2], "file"))   is_file = 1;
    else { fprintf(stderr, "vlicx: invalid mode '%s'\n", argv[2]); return 1; }

    char path[MAXPATH + MAXPATH];
    if (argv[3][0] == '/') snprintf(path, sizeof(path), "%s", argv[3]);
    else {
        char cwd[MAXPATH];
        if (getcwd(cwd, sizeof(cwd)))
            snprintf(path, sizeof(path), "%s/%s", cwd, argv[3]);
        else snprintf(path, sizeof(path), "%s", argv[3]);
    }

    struct stat st;
    if (!is_file) {
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "vlicx: folder not found: %s\n", path); return 1;
        }
    } else {
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            fprintf(stderr, "vlicx: %s is a directory (use vlicx-fo)\n", path); return 1;
        }
    }

    setlocale(LC_ALL, "");
    term_setup();

    initscr(); raw(); noecho(); keypad(stdscr, TRUE);
    curs_set(1); set_escdelay(25);
    fputs("\033[5 q", stdout); fflush(stdout); /* Request blinking bar cursor (|) */

#ifdef NCURSES_VERSION
    define_key("\033[27;5;49~", VKEY_CTRL1);
    define_key("\033[27;5;50~", VKEY_CTRL2);
    define_key("\033[49;5u",    VKEY_CTRL1);
    define_key("\033[50;5u",    VKEY_CTRL2);
#endif

    App app;
    app_init(&app, path, is_file);
    ui_init_colors(app.cfg.code_scheme, app.cfg.ui_scheme);

    app_run(&app);

    app_cleanup(&app);
    endwin();
    term_restore();

    check_update_notice();

    return 0;
}
/* vlix.h — Vlix terminal editor shared header */
#ifndef VLIX_H
#define VLIX_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700
#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <wchar.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

#define APP_NAME        "Vlicx"
#define CONFIG_FILENAME ".vlicxrc"
#define MAXPATH         4096
#define MAX_STATUS      512
#define MAX_ERRORS      64
#define TAB_STOP        4
#define EXPLORER_MIN_W  20
#define EXPLORER_DIV    3
#define TOPBAR_H        5
#define LINENO_W        6
#define LINE_BUF        8192
#define MAX_UNDO        50

/* ---- UTF-8 helpers ---- */
static inline int utf8_char_len(const char *p) {
    unsigned char c = (unsigned char)*p;
    if (c == 0) return 0;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static inline int utf8_char_col_width(const char *p) {
    unsigned char c = (unsigned char)*p;
    if (c == 0) return 0;
    if (c < 0x80) return 1;
    int len = utf8_char_len(p);
    if (len >= 3) return 2; /* 3-byte and 4-byte UTF-8 chars (Japanese CJK etc.) take 2 cols */
    return 1;
}

static inline int utf8_prev_char(const char *text, int cx) {
    if (cx <= 0) return 0;
    cx--;
    while (cx > 0 && ((unsigned char)text[cx] & 0xC0) == 0x80) {
        cx--;
    }
    return cx;
}

static inline int utf8_align_char(const char *text, int cx) {
    if (cx <= 0) return 0;
    int len = (int)strlen(text);
    if (cx > len) cx = len;
    while (cx > 0 && ((unsigned char)text[cx] & 0xC0) == 0x80) {
        cx--;
    }
    return cx;
}

#define VKEY_CTRL1 (KEY_MAX + 10)
#define VKEY_CTRL2 (KEY_MAX + 11)

/* ---- i18n ---- */
enum { LANG_EN = 0, LANG_JP = 1, LANG_COUNT };

enum {
    S_LBL_COPY, S_LBL_SAVE, S_LBL_LINECLR, S_LBL_PASTE, S_LBL_UNDO, S_LBL_EXIT,
    S_LBL_COLOR, S_LBL_MOVE, S_LBL_SEARCH, S_LBL_SELECT, S_LBL_ENDSEL,
    S_LBL_SETTING, S_LBL_ERRORS, S_LBL_HELP, S_LBL_DELETE,
    S_MSG_SAVED, S_MSG_SAVE_FAIL, S_MSG_NO_FILE, S_MSG_LINE_CLR,
    S_MSG_COPY_SEL, S_MSG_COPY_LINE, S_MSG_PASTED, S_MSG_UNDO, S_MSG_NO_UNDO,
    S_MSG_FOCUS, S_MSG_OPENED, S_MSG_DELETED, S_MSG_DEL_FAIL,
    S_MSG_SEL_ON, S_MSG_SEL_OFF, S_MSG_COLOR_CHG,
    S_MSG_NOT_FOUND, S_MSG_FOUND, S_MSG_NO_SEARCH, S_MSG_BINARY,
    S_MSG_CONFIRM,
    S_OVL_HELP_T, S_OVL_ERR_T, S_OVL_SET_T,
    S_OVL_ANYKEY, S_OVL_ARROWS,
    S_OVL_CHG_CODE_COLOR, S_OVL_CHG_UI_COLOR, S_OVL_CHG_LANG, S_OVL_CLOSE,
    S_OVL_CUR_CODE_COLOR, S_OVL_CUR_UI_COLOR, S_OVL_CUR_LANG, S_OVL_EN, S_OVL_JP,
    S_OVL_NEWFILE_T, S_OVL_NEWDIR_T, S_OVL_PROMPT_NAME, S_OVL_INPUT_HINT,
    S_MSG_CREATED_FILE, S_MSG_CREATED_DIR, S_MSG_CREATE_FAIL,
    S_TITLE_BAR, S_NO_FILE_HINT,
    S_WORD_EDITOR, S_WORD_EXPLORER, S_NO_ERRORS,
    S_COUNT
};

void        i18n_set_language(int lang);
int         i18n_get_language(void);
const char *tr(int id);

/* ---- config ---- */
typedef struct { int code_scheme; int ui_scheme; int language; } Config;
void config_load(Config *c);
void config_save(const Config *c);

/* ---- color schemes ---- */
typedef struct {
    const char *name;
    short bg, fg, accent;
    /* Syntax colors */
    short kw_fg;       /* Keywords: if, return, while, static */
    short func_fg;     /* Functions: rmtree, ln_set, snprintf */
    short type_fg;     /* Types: int, char, stat, DIR, Editor, EdLine */
    short str_fg;      /* Strings: "...", '...' */
    short cmt_fg;      /* Comments: // or block comments */
    short num_fg;      /* Numbers: 0, 100, 0x80 */
    short macro_fg;    /* Macros: NULL, MAXPATH, S_ISDIR */
    short op_fg;       /* Operators: ->, ., ==, !=, {, }, (, ) */
    short var_fg;      /* Variables: path, st, d, ent */
} ColorScheme;

enum {
    CP_TOPBAR = 1, CP_EXPLORER, CP_EXPLORER_SEL, CP_EDITOR,
    CP_ACCENT, CP_OVERLAY, CP_SELECTION, CP_LINENO, CP_STATUS,
    CP_CURLINE,
    CP_SYNTAX_KEYWORD, CP_SYNTAX_FUNCTION, CP_SYNTAX_TYPE,
    CP_SYNTAX_STRING, CP_SYNTAX_COMMENT, CP_SYNTAX_NUMBER,
    CP_SYNTAX_MACRO, CP_SYNTAX_OPERATOR, CP_SYNTAX_VARIABLE,
    CP_SYNTAX_TAG, CP_SYNTAX_ATTR, CP_SYNTAX_PREPROC
};
extern const ColorScheme g_schemes[];
extern const int         g_nschemes;
void ui_init_colors(int code_idx, int ui_idx);

/* ---- editor & undo ---- */
typedef struct { char *text; int len; int cap; int dirty; } EdLine;

typedef struct {
    char **lines;
    int nlines;
    int cx, cy;
} UndoStep;

typedef struct {
    EdLine *lines; int nlines, lcap;
    int cx, cy, top;
    char *filepath; int modified;
    int sel_start, sel_active;
    char *clipboard;
    UndoStep *undo_stack;
    int undo_count, undo_cap;
} Editor;

void  editor_init(Editor *e);
void  editor_free(Editor *e);
void  editor_load(Editor *e, const char *path);
int   editor_save(Editor *e, char *msg, int msz);
void  editor_save_undo(Editor *e);
int   editor_undo(Editor *e);
void  editor_insert_char(Editor *e, int ch);
void  editor_newline(Editor *e);
void  editor_backspace(Editor *e);
void  editor_delete_char(Editor *e);
void  editor_clear_line(Editor *e);
void  editor_move(Editor *e, int dy, int dx);
int   editor_selection(Editor *e, int *lo, int *hi);
char *editor_selected_text(Editor *e);
int   editor_search_wrap(Editor *e, const char *needle);

/* ---- explorer ---- */
typedef struct ExpNode {
    char *path, *name;
    int is_dir, expanded, depth, loaded;
    struct ExpNode **children; int nch, ccap;
} ExpNode;
typedef struct {
    ExpNode *root;
    ExpNode **flat; int fcount, fcap;
    int selected, scroll;
} Explorer;

ExpNode *expnode_new(const char *p, const char *n, int d, int dep);
void     expnode_free(ExpNode *n);
void     expnode_load(ExpNode *n);
void     expnode_toggle(ExpNode *n);
void     expnode_refresh(ExpNode *n);
void     explorer_init(Explorer *ex, const char *root);
void     explorer_free(Explorer *ex);
void     explorer_flatten(Explorer *ex);
void     explorer_move(Explorer *ex, int delta);
ExpNode *explorer_current(Explorer *ex);
void     explorer_expand(Explorer *ex);
void     explorer_collapse(Explorer *ex);
int      explorer_delete_node(Explorer *ex, char *err, int esz);

/* ---- application ---- */
enum { FOCUS_EXPLORER = 0, FOCUS_EDITOR = 1 };
enum { OVL_NONE = 0, OVL_HELP, OVL_ERRORS, OVL_SETTINGS, OVL_CONFIRM, OVL_NEW_FILE, OVL_NEW_DIR, OVL_CONVERT };

#define MAX_CONVERT_CANDS 32

typedef struct {
    char convert_query[128];
    int convert_start_cx;
    int convert_end_cx;
    char *convert_cands[MAX_CONVERT_CANDS];
    int convert_ncand;
    int convert_sel;
} ConvertState;

typedef struct {
    Editor ed; Explorer ex; Config cfg;
    int focus, running, overlay, settings_cur;
    char status[MAX_STATUS];
    char *errors[MAX_ERRORS]; int nerr;
    char confirm_name[256];
    char input_buf[256];
    int input_pos;
    char target_dir[MAXPATH];
    ConvertState conv;
} App;

void ui_draw(App *app);
void app_error(App *app, const char *msg);

/* ---- Japanese Dictionary ---- */
void dict_load(void);   /* loads SKK dictionary from $VLICX_JISYO or ~/.vlicx-jisyo/prtxt; safe to call multiple times, lazy-called by dict_lookup() */
void dict_unload(void); /* frees dictionary memory; call at app shutdown */
int  dict_lookup(const char *hiragana, char *cands[], int max_cands);
void dict_free_cands(char *cands[], int ncand);

/* ---- Editor Hiragana helpers ---- */
int  editor_find_hiragana_before_cursor(Editor *e, int *out_start_cx, int *out_end_cx, char *out_buf, int max_buf);
void editor_replace_range(Editor *e, int start_cx, int end_cx, const char *replacement);

#endif

/* config.c — i18n string table, color schemes, settings persistence */
#include "vlicx.h"

/* ===== Color Schemes ===== */
/* ===== Color Schemes ===== */
const ColorScheme g_schemes[] = {
    /* 1. VSCode Dark+ */
    {
        "VSCode Dark+",
        COLOR_BLACK, COLOR_WHITE, COLOR_YELLOW,
        75,  /* kw: Sky Blue */
        220, /* func: Bright Gold */
        43,  /* type: Teal / Mint */
        209, /* str: Peach Orange */
        71,  /* cmt: Muted Olive Green */
        150, /* num: Light Green */
        176, /* macro: Violet / Magenta */
        213, /* op: Pink */
        117  /* var: Light Blue */
    },
    /* 2. Monokai Pro */
    {
        "Monokai Pro",
        COLOR_BLACK, COLOR_WHITE, COLOR_MAGENTA,
        204, /* kw: Hot Pink */
        149, /* func: Lime Green */
        81,  /* type: Bright Cyan */
        221, /* str: Gold Yellow */
        242, /* cmt: Slate Gray */
        141, /* num: Purple */
        203, /* macro: Coral Red */
        213, /* op: Bright Pink */
        231  /* var: White */
    },
    /* 3. One Dark Pro */
    {
        "One Dark Pro",
        COLOR_BLACK, COLOR_WHITE, COLOR_BLUE,
        176, /* kw: Purple */
        75,  /* func: Bright Blue */
        180, /* type: Amber Yellow */
        114, /* str: Soft Green */
        243, /* cmt: Steel Gray */
        173, /* num: Orange */
        73,  /* macro: Cyan */
        220, /* op: Yellow */
        117  /* var: Light Blue */
    },
    /* 4. Dracula */
    {
        "Dracula",
        COLOR_BLACK, COLOR_WHITE, COLOR_MAGENTA,
        212, /* kw: Neon Pink */
        84,  /* func: Neon Green */
        117, /* type: Cyan */
        228, /* str: Light Yellow */
        61,  /* cmt: Purple Gray */
        141, /* num: Purple */
        215, /* macro: Orange */
        213, /* op: Pink */
        231  /* var: White */
    },
    /* 5. Tokyo Night */
    {
        "Tokyo Night",
        COLOR_BLACK, COLOR_WHITE, COLOR_CYAN,
        117, /* kw: Light Blue */
        75,  /* func: Blue */
        44,  /* type: Deep Cyan */
        114, /* str: Green */
        60,  /* cmt: Slate Gray */
        215, /* num: Orange */
        141, /* macro: Violet */
        213, /* op: Pink */
        159  /* var: Ice Blue */
    },
    /* 6. SynthWave '84 */
    {
        "SynthWave '84",
        COLOR_BLACK, COLOR_WHITE, COLOR_YELLOW,
        207, /* kw: Neon Pink */
        220, /* func: Neon Gold */
        48,  /* type: Emerald Green */
        209, /* str: Coral Orange */
        97,  /* cmt: Deep Purple */
        121, /* num: Neon Cyan */
        165, /* macro: Purple */
        220, /* op: Gold */
        117  /* var: Light Blue */
    },
    /* 7. GitHub Dark */
    {
        "GitHub Dark",
        COLOR_BLACK, COLOR_WHITE, COLOR_RED,
        203, /* kw: Coral Red */
        141, /* func: Purple */
        75,  /* type: Light Blue */
        117, /* str: Cyan */
        244, /* cmt: Gray */
        215, /* num: Orange */
        114, /* macro: Green */
        220, /* op: Yellow */
        231  /* var: White */
    },
    /* 8. Nordic Frost */
    {
        "Nordic Frost",
        COLOR_BLACK, COLOR_WHITE, COLOR_CYAN,
        110, /* kw: Frost Blue */
        73,  /* func: Soft Cyan */
        109, /* type: Ice Blue */
        108, /* str: Sage Green */
        66,  /* cmt: Slate Blue */
        139, /* num: Aurora Purple */
        167, /* macro: Aurora Red */
        220, /* op: Gold */
        231  /* var: White */
    }
};
const int g_nschemes = 8;

static short safe_color(short c256, short fallback16) {
    if (COLORS >= 256) return c256;
    return fallback16;
}

void ui_init_colors(int code_idx, int ui_idx) {
    start_color();
    use_default_colors();
    if (code_idx < 0 || code_idx >= g_nschemes) code_idx = 0;
    if (ui_idx   < 0 || ui_idx   >= g_nschemes) ui_idx   = 0;
    const ColorScheme *cs = &g_schemes[code_idx];
    const ColorScheme *us = &g_schemes[ui_idx];

    /* UI panel colors (Fixed WHITE text for high contrast & legibility) */
    init_pair(CP_TOPBAR,         COLOR_WHITE, us->accent);
    init_pair(CP_EXPLORER,       COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_EXPLORER_SEL,   COLOR_WHITE, us->accent);
    init_pair(CP_EDITOR,         safe_color(us->var_fg, COLOR_WHITE), COLOR_BLACK);
    init_pair(CP_ACCENT,         COLOR_WHITE, us->accent);
    init_pair(CP_OVERLAY,        COLOR_WHITE, COLOR_BLUE);
    init_pair(CP_SELECTION,      COLOR_WHITE, COLOR_BLUE);
    init_pair(CP_LINENO,         safe_color(us->accent, COLOR_CYAN), COLOR_BLACK);
    init_pair(CP_STATUS,         COLOR_WHITE, us->bg);
    init_pair(CP_CURLINE,        COLOR_YELLOW, COLOR_BLACK);

    /* Coding syntax colors (use Code scheme) */
    init_pair(CP_SYNTAX_KEYWORD,  safe_color(cs->kw_fg,    COLOR_BLUE),    COLOR_BLACK);
    init_pair(CP_SYNTAX_FUNCTION, safe_color(cs->func_fg,  COLOR_YELLOW),  COLOR_BLACK);
    init_pair(CP_SYNTAX_TYPE,     safe_color(cs->type_fg,  COLOR_CYAN),    COLOR_BLACK);
    init_pair(CP_SYNTAX_STRING,   safe_color(cs->str_fg,    COLOR_MAGENTA), COLOR_BLACK);
    init_pair(CP_SYNTAX_COMMENT,  safe_color(cs->cmt_fg,   COLOR_GREEN),   COLOR_BLACK);
    init_pair(CP_SYNTAX_NUMBER,   safe_color(cs->num_fg,   COLOR_GREEN),   COLOR_BLACK);
    init_pair(CP_SYNTAX_MACRO,    safe_color(cs->macro_fg, COLOR_MAGENTA), COLOR_BLACK);
    init_pair(CP_SYNTAX_OPERATOR, safe_color(cs->op_fg,    COLOR_YELLOW),  COLOR_BLACK);
    init_pair(CP_SYNTAX_VARIABLE, safe_color(cs->var_fg,   COLOR_WHITE),   COLOR_BLACK);
    init_pair(CP_SYNTAX_TAG,      safe_color(cs->type_fg,  COLOR_CYAN),    COLOR_BLACK);
    init_pair(CP_SYNTAX_ATTR,     safe_color(cs->func_fg,  COLOR_YELLOW),  COLOR_BLACK);
    init_pair(CP_SYNTAX_PREPROC,  safe_color(cs->macro_fg, COLOR_MAGENTA), COLOR_BLACK);
}

/* ===== i18n ===== */
static int g_lang = LANG_EN;

static const char *g_str[S_COUNT][LANG_COUNT] = {
    /* --- shortcut labels --- */
    [S_LBL_COPY]    = { "Copy",    "コピー" },
    [S_LBL_SAVE]    = { "Save",    "保存" },
    [S_LBL_LINECLR] = { "LinClr",  "行削除" },
    [S_LBL_PASTE]   = { "Paste",   "貼付" },
    [S_LBL_UNDO]    = { "Undo",    "元に戻す" },
    [S_LBL_EXIT]    = { "Exit",    "終了" },
    [S_LBL_COLOR]   = { "Color",   "配色" },
    [S_LBL_MOVE]    = { "Move",    "移動" },
    [S_LBL_SEARCH]  = { "Search",  "検索" },
    [S_LBL_SELECT]  = { "Select",  "選択" },
    [S_LBL_ENDSEL]  = { "EndSel",  "選択終" },
    [S_LBL_SETTING] = { "Setting", "設定" },
    [S_LBL_ERRORS]  = { "Errors",  "エラー" },
    [S_LBL_HELP]    = { "Help",    "ヘルプ" },
    [S_LBL_DELETE]  = { "Delete",  "削除" },
    /* --- status messages --- */
    [S_MSG_SAVED]     = { "Saved: %s",            "保存しました: %s" },
    [S_MSG_SAVE_FAIL] = { "Save failed: %s",      "保存失敗: %s" },
    [S_MSG_NO_FILE]   = { "No file to save",      "保存先が未指定" },
    [S_MSG_LINE_CLR]  = { "Line cleared",         "行を削除" },
    [S_MSG_COPY_SEL]  = { "Selection copied",     "選択範囲コピー" },
    [S_MSG_COPY_LINE] = { "Line copied",          "現在行コピー" },
    [S_MSG_PASTED]    = { "Pasted",               "貼り付け" },
    [S_MSG_UNDO]      = { "Undo executed",        "元に戻しました" },
    [S_MSG_NO_UNDO]   = { "Nothing to undo",      "この上戻せません" },
    [S_MSG_FOCUS]     = { "Focus: %s",            "フォーカス: %s" },
    [S_MSG_OPENED]    = { "Opened: %s",           "開きました: %s" },
    [S_MSG_DELETED]   = { "Deleted",              "削除しました" },
    [S_MSG_DEL_FAIL]  = { "Delete failed: %s",    "削除失敗: %s" },
    [S_MSG_SEL_ON]    = { "Selection mode",       "選択モード開始" },
    [S_MSG_SEL_OFF]   = { "End selection (kept)", "選択終了（維持）" },
    [S_MSG_COLOR_CHG] = { "Color: %s",            "配色: %s" },
    [S_MSG_NOT_FOUND] = { "Not found",            "見つかりません" },
    [S_MSG_FOUND]     = { "Found: line %d",       "見つかりました: %d行目" },
    [S_MSG_NO_SEARCH] = { "No search text",       "検索テキストなし" },
    [S_MSG_BINARY]    = { "<Cannot display>",     "<表示不可>" },
    [S_MSG_CONFIRM]   = { "Delete \"%s\"? (y/N)", "\"%s\" を削除? (y/N)" },
    /* --- overlay --- */
    [S_OVL_HELP_T]        = { "--- Vlicx Help ---",            "--- Vlicx ヘルプ ---" },
    [S_OVL_ERR_T]         = { "--- Errors ---",                "--- エラー表示 ---" },
    [S_OVL_SET_T]         = { "--- Vlicx Settings ---",         "--- Vlicx 設定 ---" },
    [S_OVL_ANYKEY]        = { "(Press any key)",               "(何かキーを押す)" },
    [S_OVL_ARROWS]        = { "(Up/Down Enter/Esc)",           "(Up/Down Enter/Esc)" },
    [S_OVL_CHG_CODE_COLOR]= { "Change code color scheme",      "コーディング配色を切り替え" },
    [S_OVL_CHG_UI_COLOR]  = { "Change UI color scheme",        "操作盤配色を切り替え" },
    [S_OVL_CHG_LANG]      = { "Change language",               "言語を変更" },
    [S_OVL_CLOSE]         = { "Close",                         "閉じる" },
    [S_OVL_CUR_CODE_COLOR]= { "Code Color: %s",                "コード配色: %s" },
    [S_OVL_CUR_UI_COLOR]  = { "UI Color: %s",                  "操作盤配色: %s" },
    [S_OVL_CUR_LANG]      = { "Language: %s",                  "言語: %s" },
    [S_OVL_EN]            = { "English",                       "English" },
    [S_OVL_JP]            = { "Japanese",                      "日本語" },
    [S_OVL_NEWFILE_T]     = { "--- New File ---",              "--- 新規ファイル作成 ---" },
    [S_OVL_NEWDIR_T]      = { "--- New Folder ---",            "--- 新規フォルダ作成 ---" },
    [S_OVL_PROMPT_NAME]   = { "Name:",                         "名前:" },
    [S_OVL_INPUT_HINT]    = { "[Enter] Create  [Esc] Cancel",  "[Enter] 作成  [Esc] キャンセル" },
    [S_MSG_CREATED_FILE]  = { "Created file: %s",            "ファイルを作成しました: %s" },
    [S_MSG_CREATED_DIR]   = { "Created folder: %s",          "フォルダを作成しました: %s" },
    [S_MSG_CREATE_FAIL]   = { "Create failed: %s",           "作成失敗: %s" },
    /* --- general --- */
    [S_TITLE_BAR]         = { " Vlicx - Shortcuts ",            " Vlicx - 基本操作 " },
    [S_NO_FILE_HINT]      = { "(No file - select from Explorer)", "(ファイル未選択)" },
    [S_WORD_EDITOR]       = { "Editor",                        "エディタ" },
    [S_WORD_EXPLORER]     = { "Explorer",                      "エクスプローラ" },
    [S_NO_ERRORS]         = { "(No errors)",                   "(エラーなし)" },
};

void i18n_set_language(int l) { if(l>=0&&l<LANG_COUNT) g_lang=l; }
int  i18n_get_language(void)  { return g_lang; }
const char *tr(int id) {
    if (id<0||id>=S_COUNT) return "?";
    const char *s = g_str[id][g_lang];
    return s ? s : g_str[id][LANG_EN];
}

/* ===== Config load/save ===== */
void config_load(Config *c) {
    c->code_scheme = 0; c->ui_scheme = 0; c->language = LANG_EN;
    const char *home = getenv("HOME");
    if (!home) return;
    char path[MAXPATH];
    snprintf(path, sizeof(path), "%s/%s", home, CONFIG_FILENAME);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0'; char *val = eq+1;
        char *nl = strchr(val, '\n'); if(nl) *nl='\0';
        if (!strcmp(line,"code_scheme")) c->code_scheme = atoi(val);
        else if (!strcmp(line,"ui_scheme")) c->ui_scheme = atoi(val);
        else if (!strcmp(line,"color_scheme")) { c->code_scheme = atoi(val); c->ui_scheme = atoi(val); }
        else if (!strcmp(line,"language")) c->language = atoi(val);
    }
    fclose(f);
    if (c->code_scheme<0||c->code_scheme>=g_nschemes) c->code_scheme=0;
    if (c->ui_scheme<0||c->ui_scheme>=g_nschemes) c->ui_scheme=0;
    if (c->language<0||c->language>=LANG_COUNT) c->language=LANG_EN;
}

void config_save(const Config *c) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[MAXPATH];
    snprintf(path, sizeof(path), "%s/%s", home, CONFIG_FILENAME);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "code_scheme=%d\nui_scheme=%d\nlanguage=%d\n", c->code_scheme, c->ui_scheme, c->language);
    fclose(f);
}

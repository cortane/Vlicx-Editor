/* explorer.c — file/folder tree with lazy loading */
#include "vlix.h"

/* ---- recursive delete (rm -rf) ---- */
static int rmtree(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return -1;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
            char child[MAXPATH];
            snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            if (rmtree(child) != 0) { closedir(d); return -1; }
        }
        closedir(d);
        return rmdir(path);
    }
    return unlink(path);
}

/* ---- sort: dirs first, then case-insensitive alpha ---- */
static int node_cmp(const void *a, const void *b) {
    const ExpNode *na = *(const ExpNode *const *)a;
    const ExpNode *nb = *(const ExpNode *const *)b;
    if (na->is_dir != nb->is_dir) return nb->is_dir - na->is_dir;
    return strcasecmp(na->name, nb->name);
}

/* ---- find parent of a node in tree ---- */
static ExpNode *find_parent(ExpNode *root, ExpNode *target) {
    for (int i = 0; i < root->nch; i++) {
        if (root->children[i] == target) return root;
        if (root->children[i]->is_dir && root->children[i]->expanded) {
            ExpNode *p = find_parent(root->children[i], target);
            if (p) return p;
        }
    }
    return NULL;
}

/* ---- remove child from parent without re-scanning disk ---- */
static void remove_child(ExpNode *parent, ExpNode *child) {
    for (int i = 0; i < parent->nch; i++) {
        if (parent->children[i] == child) {
            expnode_free(child);
            memmove(&parent->children[i], &parent->children[i + 1],
                    sizeof(ExpNode *) * (parent->nch - i - 1));
            parent->nch--;
            return;
        }
    }
}

/* ==== Node ==== */
ExpNode *expnode_new(const char *path, const char *name, int is_dir, int depth) {
    ExpNode *n = (ExpNode *)calloc(1, sizeof(ExpNode));
    n->path = strdup(path);
    n->name = strdup(name);
    n->is_dir = is_dir;
    n->depth = depth;
    return n;
}

void expnode_free(ExpNode *n) {
    if (!n) return;
    for (int i = 0; i < n->nch; i++) expnode_free(n->children[i]);
    free(n->children);
    free(n->path);
    free(n->name);
    free(n);
}

void expnode_load(ExpNode *n) {
    if (n->loaded || !n->is_dir) return;
    n->loaded = 1;
    DIR *d = opendir(n->path);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;
        char fp[MAXPATH];
        snprintf(fp, sizeof(fp), "%s/%s", n->path, ent->d_name);
        struct stat st;
        int isdir = 0;
        if (stat(fp, &st) == 0) isdir = S_ISDIR(st.st_mode);
        if (n->nch >= n->ccap) {
            n->ccap = n->ccap ? n->ccap * 2 : 32;
            n->children = (ExpNode **)realloc(n->children,
                                              sizeof(ExpNode *) * n->ccap);
        }
        n->children[n->nch++] = expnode_new(fp, ent->d_name, isdir, n->depth + 1);
    }
    closedir(d);
    if (n->nch > 1)
        qsort(n->children, n->nch, sizeof(ExpNode *), node_cmp);
}

void expnode_toggle(ExpNode *n) {
    if (!n->is_dir) return;
    if (!n->loaded) expnode_load(n);
    n->expanded = !n->expanded;
}

void expnode_refresh(ExpNode *n) {
    for (int i = 0; i < n->nch; i++) expnode_free(n->children[i]);
    n->nch = 0; n->loaded = 0;
    if (n->expanded) expnode_load(n);
}

/* ==== Explorer ==== */
static void flatten_r(Explorer *ex, ExpNode *n) {
    if (ex->fcount >= ex->fcap) {
        ex->fcap = ex->fcap ? ex->fcap * 2 : 256;
        ex->flat = (ExpNode **)realloc(ex->flat, sizeof(ExpNode *) * ex->fcap);
    }
    ex->flat[ex->fcount++] = n;
    if (n->is_dir && n->expanded) {
        for (int i = 0; i < n->nch; i++) flatten_r(ex, n->children[i]);
    }
}

void explorer_init(Explorer *ex, const char *root) {
    memset(ex, 0, sizeof(*ex));
    const char *name = strrchr(root, '/');
    name = name ? name + 1 : root;
    if (!*name) name = root;
    ex->root = expnode_new(root, name, 1, 0);
    ex->root->expanded = 1;
    expnode_load(ex->root);
    explorer_flatten(ex);
}

void explorer_free(Explorer *ex) {
    expnode_free(ex->root);
    free(ex->flat);
    memset(ex, 0, sizeof(*ex));
}

void explorer_flatten(Explorer *ex) {
    ex->fcount = 0;
    if (ex->root) flatten_r(ex, ex->root);
}

void explorer_move(Explorer *ex, int delta) {
    explorer_flatten(ex);
    if (!ex->fcount) return;
    ex->selected += delta;
    if (ex->selected < 0) ex->selected = 0;
    if (ex->selected >= ex->fcount) ex->selected = ex->fcount - 1;
}

ExpNode *explorer_current(Explorer *ex) {
    explorer_flatten(ex);
    if (!ex->fcount || ex->selected >= ex->fcount) return NULL;
    return ex->flat[ex->selected];
}

void explorer_expand(Explorer *ex) {
    ExpNode *n = explorer_current(ex);
    if (n && n->is_dir && !n->expanded) expnode_toggle(n);
    explorer_flatten(ex);
}

void explorer_collapse(Explorer *ex) {
    ExpNode *n = explorer_current(ex);
    if (n && n->is_dir && n->expanded) expnode_toggle(n);
    explorer_flatten(ex);
}

int explorer_delete_node(Explorer *ex, char *err, int esz) {
    ExpNode *n = explorer_current(ex);
    if (!n || n == ex->root) return -1;
    if (rmtree(n->path) != 0) {
        snprintf(err, esz, "%s", strerror(errno));
        return -1;
    }
    ExpNode *parent = find_parent(ex->root, n);
    if (parent) remove_child(parent, n);
    else expnode_refresh(ex->root);
    explorer_flatten(ex);
    if (ex->selected >= ex->fcount) ex->selected = ex->fcount - 1;
    if (ex->selected < 0) ex->selected = 0;
    return 0;
}

#include "fmc_data.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FMC_APT_PATH "assets/assets/assets/apt.dat"
#define FMC_FIX_PATH "assets/assets/assets/earth_fix.dat"
#define FMC_FMS_PATH "assets/assets/assets/KSEAKBFI.fms"

struct FMCAirportNode {
    FMCAirportRecord record;
    int height;
    struct FMCAirportNode *left;
    struct FMCAirportNode *right;
};

struct FMCFixNode {
    FMCFixRecord record;
    int height;
    struct FMCFixNode *left;
    struct FMCFixNode *right;
};

FMCPage fmc_current_page = FMC_PAGE_MENU;
int fmc_route_page = 0;
int fmc_dep_arr_page = 0;
int fmc_exec_armed = 0;
char fmc_scratchpad[FMC_SCRATCHPAD_LEN] = "";
char fmc_status_message[64] = "READY";
char fmc_origin[FMC_AIRPORT_CODE_LEN] = "----";
char fmc_destination[FMC_AIRPORT_CODE_LEN] = "----";
char fmc_pending_origin[FMC_AIRPORT_CODE_LEN] = "";
char fmc_pending_destination[FMC_AIRPORT_CODE_LEN] = "";
FMCRouteEntry fmc_route[FMC_ROUTE_CAPACITY];
size_t fmc_route_count = 0;
FMCPhaseData fmc_climb_data = {250, 12000, 18000, 0};
FMCPhaseData fmc_cruise_data = {450, 35000, 0, 0};
FMCPhaseData fmc_descent_data = {280, 3000, 0, 140};
FMCProcedureOptions fmc_proc_options;
int fmc_airport_count = 0;
int fmc_fix_count = 0;

/* Airports and fixes are indexed separately so scratchpad lookups stay fast. */
static FMCAirportNode *g_airport_root = NULL;
static FMCFixNode *g_fix_root = NULL;

static int max_int(int a, int b) { return a > b ? a : b; }

static void uppercase_copy(char *dst, size_t size, const char *src) {
    size_t i;

    if (size == 0) return;
    for (i = 0; i + 1 < size && src[i] != '\0'; ++i)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

static int compare_codes(const char *a, const char *b) {
    return _stricmp(a, b);
}

static int airport_height(FMCAirportNode *node) { return node == NULL ? 0 : node->height; }
static int fix_height(FMCFixNode *node) { return node == NULL ? 0 : node->height; }

static FMCAirportNode *rotate_airport_right(FMCAirportNode *y) {
    FMCAirportNode *x = y->left;
    FMCAirportNode *t2 = x->right;

    x->right = y;
    y->left = t2;
    y->height = max_int(airport_height(y->left), airport_height(y->right)) + 1;
    x->height = max_int(airport_height(x->left), airport_height(x->right)) + 1;
    return x;
}

static FMCAirportNode *rotate_airport_left(FMCAirportNode *x) {
    FMCAirportNode *y = x->right;
    FMCAirportNode *t2 = y->left;

    y->left = x;
    x->right = t2;
    x->height = max_int(airport_height(x->left), airport_height(x->right)) + 1;
    y->height = max_int(airport_height(y->left), airport_height(y->right)) + 1;
    return y;
}

static FMCFixNode *rotate_fix_right(FMCFixNode *y) {
    FMCFixNode *x = y->left;
    FMCFixNode *t2 = x->right;

    x->right = y;
    y->left = t2;
    y->height = max_int(fix_height(y->left), fix_height(y->right)) + 1;
    x->height = max_int(fix_height(x->left), fix_height(x->right)) + 1;
    return x;
}

static FMCFixNode *rotate_fix_left(FMCFixNode *x) {
    FMCFixNode *y = x->right;
    FMCFixNode *t2 = y->left;

    y->left = x;
    x->right = t2;
    x->height = max_int(fix_height(x->left), fix_height(x->right)) + 1;
    y->height = max_int(fix_height(y->left), fix_height(y->right)) + 1;
    return y;
}

static FMCAirportNode *insert_airport(FMCAirportNode *node, const FMCAirportRecord *record) {
    int balance;
    int cmp;

    if (node == NULL) {
        FMCAirportNode *new_node = (FMCAirportNode *)calloc(1, sizeof(FMCAirportNode));
        if (new_node == NULL) return NULL;
        new_node->record = *record;
        new_node->height = 1;
        ++fmc_airport_count;
        return new_node;
    }

    cmp = compare_codes(record->code, node->record.code);
    if (cmp < 0) node->left = insert_airport(node->left, record);
    else if (cmp > 0) node->right = insert_airport(node->right, record);
    else return node;

    node->height = max_int(airport_height(node->left), airport_height(node->right)) + 1;
    balance = airport_height(node->left) - airport_height(node->right);

    if (balance > 1 && compare_codes(record->code, node->left->record.code) < 0) return rotate_airport_right(node);
    if (balance < -1 && compare_codes(record->code, node->right->record.code) > 0) return rotate_airport_left(node);
    if (balance > 1 && compare_codes(record->code, node->left->record.code) > 0) {
        node->left = rotate_airport_left(node->left);
        return rotate_airport_right(node);
    }
    if (balance < -1 && compare_codes(record->code, node->right->record.code) < 0) {
        node->right = rotate_airport_right(node->right);
        return rotate_airport_left(node);
    }
    return node;
}

static FMCFixNode *insert_fix(FMCFixNode *node, const FMCFixRecord *record) {
    int balance;
    int cmp;

    if (node == NULL) {
        FMCFixNode *new_node = (FMCFixNode *)calloc(1, sizeof(FMCFixNode));
        if (new_node == NULL) return NULL;
        new_node->record = *record;
        new_node->height = 1;
        ++fmc_fix_count;
        return new_node;
    }

    cmp = compare_codes(record->name, node->record.name);
    if (cmp < 0) node->left = insert_fix(node->left, record);
    else if (cmp > 0) node->right = insert_fix(node->right, record);
    else return node;

    node->height = max_int(fix_height(node->left), fix_height(node->right)) + 1;
    balance = fix_height(node->left) - fix_height(node->right);

    if (balance > 1 && compare_codes(record->name, node->left->record.name) < 0) return rotate_fix_right(node);
    if (balance < -1 && compare_codes(record->name, node->right->record.name) > 0) return rotate_fix_left(node);
    if (balance > 1 && compare_codes(record->name, node->left->record.name) > 0) {
        node->left = rotate_fix_left(node->left);
        return rotate_fix_right(node);
    }
    if (balance < -1 && compare_codes(record->name, node->right->record.name) < 0) {
        node->right = rotate_fix_right(node->right);
        return rotate_fix_left(node);
    }
    return node;
}

static const FMCAirportRecord *find_airport_node(FMCAirportNode *node, const char *code) {
    while (node != NULL) {
        int cmp = compare_codes(code, node->record.code);
        if (cmp == 0) return &node->record;
        node = cmp < 0 ? node->left : node->right;
    }
    return NULL;
}

static const FMCFixRecord *find_fix_node(FMCFixNode *node, const char *name) {
    while (node != NULL) {
        int cmp = compare_codes(name, node->record.name);
        if (cmp == 0) return &node->record;
        node = cmp < 0 ? node->left : node->right;
    }
    return NULL;
}

static void free_airport_tree(FMCAirportNode *node) {
    if (node == NULL) return;
    free_airport_tree(node->left);
    free_airport_tree(node->right);
    free(node);
}

static void free_fix_tree(FMCFixNode *node) {
    if (node == NULL) return;
    free_fix_tree(node->left);
    free_fix_tree(node->right);
    free(node);
}

static int parse_airports(const char *path) {
    FILE *file = fopen(path, "r");
    char line[256];

    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        FMCAirportRecord record;
        char code[FMC_AIRPORT_CODE_LEN];
        double lat;
        double lon;

        if (sscanf(line, "%7s %lf %lf", code, &lat, &lon) != 3) continue;
        if (strlen(code) < 3) continue;
        uppercase_copy(record.code, sizeof(record.code), code);
        record.latitude = lat;
        record.longitude = lon;
        g_airport_root = insert_airport(g_airport_root, &record);
    }
    fclose(file);
    return 1;
}

static int parse_fixes(const char *path) {
    FILE *file = fopen(path, "r");
    char line[256];

    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        FMCFixRecord record;
        char name[FMC_FIX_NAME_LEN];
        double lat;
        double lon;

        if (sscanf(line, "%lf %lf %7s", &lat, &lon, name) != 3) continue;
        if (strlen(name) < 3) continue;
        uppercase_copy(record.name, sizeof(record.name), name);
        record.latitude = lat;
        record.longitude = lon;
        g_fix_root = insert_fix(g_fix_root, &record);
    }
    fclose(file);
    return 1;
}

static void set_status(const char *message) {
    snprintf(fmc_status_message, sizeof(fmc_status_message), "%s", message);
}

static void populate_procedure_options(void) {
    const char *origin = strcmp(fmc_origin, "----") == 0 ? "ORIG" : fmc_origin;
    const char *destination = strcmp(fmc_destination, "----") == 0 ? "DEST" : fmc_destination;

    memset(&fmc_proc_options, 0, sizeof(fmc_proc_options));
    snprintf(fmc_proc_options.dep_runways[0], FMC_FIX_NAME_LEN, "RW16");
    snprintf(fmc_proc_options.dep_runways[1], FMC_FIX_NAME_LEN, "RW34");
    snprintf(fmc_proc_options.dep_sids[0], FMC_FIX_NAME_LEN, "%.3s1", origin);
    snprintf(fmc_proc_options.dep_sids[1], FMC_FIX_NAME_LEN, "%.3s2", origin);
    snprintf(fmc_proc_options.dep_transitions[0], FMC_FIX_NAME_LEN, "TRN1");
    snprintf(fmc_proc_options.dep_transitions[1], FMC_FIX_NAME_LEN, "TRN2");
    snprintf(fmc_proc_options.arr_runways[0], FMC_FIX_NAME_LEN, "RW14");
    snprintf(fmc_proc_options.arr_runways[1], FMC_FIX_NAME_LEN, "RW32");
    snprintf(fmc_proc_options.arr_stars[0], FMC_FIX_NAME_LEN, "%.3sA", destination);
    snprintf(fmc_proc_options.arr_stars[1], FMC_FIX_NAME_LEN, "%.3sB", destination);
    snprintf(fmc_proc_options.arr_transitions[0], FMC_FIX_NAME_LEN, "IAF1");
    snprintf(fmc_proc_options.arr_transitions[1], FMC_FIX_NAME_LEN, "IAF2");
    fmc_proc_options.dep_runway_count = 2;
    fmc_proc_options.dep_sid_count = 2;
    fmc_proc_options.dep_transition_count = 2;
    fmc_proc_options.arr_runway_count = 2;
    fmc_proc_options.arr_star_count = 2;
    fmc_proc_options.arr_transition_count = 2;
}

static void set_route_entry(size_t index, const char *name, double lat, double lon, const char *kind) {
    if (index >= FMC_ROUTE_CAPACITY) return;
    uppercase_copy(fmc_route[index].name, sizeof(fmc_route[index].name), name);
    fmc_route[index].latitude = lat;
    fmc_route[index].longitude = lon;
    snprintf(fmc_route[index].kind, sizeof(fmc_route[index].kind), "%s", kind);
}

/* Seed the route page with a demo company route so paging is visible on startup. */
static int load_fms_demo(const char *path) {
    FILE *file = fopen(path, "r");
    char line[256];

    if (file == NULL) return 0;
    fmc_route_count = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        int type = 0;
        char ident[FMC_FIX_NAME_LEN];
        double lat = 0.0;
        double lon = 0.0;

        if (sscanf(line, "%d %7s %*d %lf %lf", &type, ident, &lat, &lon) != 4) continue;
        if (type != 1 && type != 11) continue;
        if (fmc_route_count >= FMC_ROUTE_CAPACITY) break;
        set_route_entry(fmc_route_count++, ident, lat, lon, type == 1 ? "APT" : "FIX");
    }
    fclose(file);
    return 1;
}

void fmc_init_state(void) {
    fmc_current_page = FMC_PAGE_MENU;
    fmc_route_page = 0;
    fmc_dep_arr_page = 0;
    fmc_exec_armed = 0;
    fmc_scratchpad[0] = '\0';
    fmc_pending_origin[0] = '\0';
    fmc_pending_destination[0] = '\0';
    snprintf(fmc_origin, sizeof(fmc_origin), "KSEA");
    snprintf(fmc_destination, sizeof(fmc_destination), "KBFI");
    fmc_climb_data.speed = 250;
    fmc_climb_data.altitude = 12000;
    fmc_climb_data.transition_altitude = 18000;
    fmc_climb_data.vref = 0;
    fmc_cruise_data.speed = 450;
    fmc_cruise_data.altitude = 35000;
    fmc_cruise_data.transition_altitude = 0;
    fmc_cruise_data.vref = 0;
    fmc_descent_data.speed = 280;
    fmc_descent_data.altitude = 3000;
    fmc_descent_data.transition_altitude = 0;
    fmc_descent_data.vref = 140;
    set_status("READY");
    populate_procedure_options();
}

int fmc_load_navigation_data(const char *apt_path, const char *fix_path, const char *fms_path) {
    const char *airport_path = apt_path == NULL ? FMC_APT_PATH : apt_path;
    const char *fixes_path = fix_path == NULL ? FMC_FIX_PATH : fix_path;
    const char *route_path = fms_path == NULL ? FMC_FMS_PATH : fms_path;
    int ok = 1;

    fmc_airport_count = 0;
    fmc_fix_count = 0;
    free_airport_tree(g_airport_root);
    free_fix_tree(g_fix_root);
    g_airport_root = NULL;
    g_fix_root = NULL;

    ok &= parse_airports(airport_path);
    ok &= parse_fixes(fixes_path);
    ok &= load_fms_demo(route_path);
    populate_procedure_options();
    set_status(ok ? "NAV DATA LOADED" : "NAV LOAD FAILED");
    return ok;
}

void fmc_free_navigation_data(void) {
    free_airport_tree(g_airport_root);
    free_fix_tree(g_fix_root);
    g_airport_root = NULL;
    g_fix_root = NULL;
}

const FMCAirportRecord *fmc_find_airport(const char *code) {
    char normalized[FMC_AIRPORT_CODE_LEN];
    uppercase_copy(normalized, sizeof(normalized), code);
    return find_airport_node(g_airport_root, normalized);
}

const FMCFixRecord *fmc_find_fix(const char *name) {
    char normalized[FMC_FIX_NAME_LEN];
    uppercase_copy(normalized, sizeof(normalized), name);
    return find_fix_node(g_fix_root, normalized);
}

int fmc_stage_airport(int is_origin, const char *code) {
    const FMCAirportRecord *airport = fmc_find_airport(code);

    if (airport == NULL) {
        set_status("NOT IN DATABASE");
        return 0;
    }
    if (is_origin) uppercase_copy(fmc_pending_origin, sizeof(fmc_pending_origin), airport->code);
    else uppercase_copy(fmc_pending_destination, sizeof(fmc_pending_destination), airport->code);
    fmc_exec_armed = 1;
    set_status(is_origin ? "ORIGIN STAGED" : "DEST STAGED");
    return 1;
}

int fmc_commit_exec(void) {
    /* Match FMC workflow: changes are staged first, then activated with EXEC. */
    if (!fmc_exec_armed) {
        set_status("NO MODIFICATION");
        return 0;
    }
    if (fmc_pending_origin[0] != '\0') {
        uppercase_copy(fmc_origin, sizeof(fmc_origin), fmc_pending_origin);
        fmc_pending_origin[0] = '\0';
    }
    if (fmc_pending_destination[0] != '\0') {
        uppercase_copy(fmc_destination, sizeof(fmc_destination), fmc_pending_destination);
        fmc_pending_destination[0] = '\0';
    }
    fmc_exec_armed = 0;
    populate_procedure_options();
    set_status("ROUTE ACTIVATED");
    return 1;
}

int fmc_append_route_waypoint(const char *name) {
    const FMCFixRecord *fix = fmc_find_fix(name);

    if (fix == NULL) {
        const FMCAirportRecord *airport = fmc_find_airport(name);
        if (airport == NULL) {
            set_status("WPT NOT FOUND");
            return 0;
        }
        if (fmc_route_count >= FMC_ROUTE_CAPACITY) {
            set_status("ROUTE FULL");
            return 0;
        }
        set_route_entry(fmc_route_count++, airport->code, airport->latitude, airport->longitude, "APT");
        set_status("AIRPORT ADDED");
        return 1;
    }

    if (fmc_route_count >= FMC_ROUTE_CAPACITY) {
        set_status("ROUTE FULL");
        return 0;
    }
    set_route_entry(fmc_route_count++, fix->name, fix->latitude, fix->longitude, "FIX");
    set_status("WAYPOINT ADDED");
    return 1;
}

void fmc_clear_scratchpad(void) {
    fmc_scratchpad[0] = '\0';
}

void fmc_backspace_scratchpad(void) {
    size_t len = strlen(fmc_scratchpad);
    if (len > 0) fmc_scratchpad[len - 1] = '\0';
}

void fmc_append_scratchpad_text(const char *text) {
    size_t len = strlen(fmc_scratchpad);
    size_t i;

    if (text == NULL) return;
    for (i = 0; text[i] != '\0' && len + 1 < sizeof(fmc_scratchpad); ++i)
        fmc_scratchpad[len++] = (char)toupper((unsigned char)text[i]);
    fmc_scratchpad[len] = '\0';
}

void fmc_cycle_dep_arr_option(int slot, int direction) {
    int *index = NULL;
    int count = 0;

    switch (slot) {
        case 0: index = &fmc_proc_options.dep_runway_index; count = fmc_proc_options.dep_runway_count; break;
        case 1: index = &fmc_proc_options.dep_sid_index; count = fmc_proc_options.dep_sid_count; break;
        case 2: index = &fmc_proc_options.dep_transition_index; count = fmc_proc_options.dep_transition_count; break;
        case 3: index = &fmc_proc_options.arr_runway_index; count = fmc_proc_options.arr_runway_count; break;
        case 4: index = &fmc_proc_options.arr_star_index; count = fmc_proc_options.arr_star_count; break;
        case 5: index = &fmc_proc_options.arr_transition_index; count = fmc_proc_options.arr_transition_count; break;
        default: break;
    }
    if (index == NULL || count == 0) return;
    *index = (*index + direction + count) % count;
    set_status("PROC OPTION CHANGED");
}

void fmc_set_page(FMCPage page) {
    fmc_current_page = page;
    if (page == FMC_PAGE_RTE) fmc_route_page = 0;
    if (page == FMC_PAGE_DEP_ARR) fmc_dep_arr_page = 0;
}

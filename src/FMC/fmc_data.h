#ifndef FMC_DATA_H
#define FMC_DATA_H

#include <stddef.h>

#define FMC_AIRPORT_CODE_LEN 8
#define FMC_FIX_NAME_LEN 8
#define FMC_ROUTE_CAPACITY 64
#define FMC_SCRATCHPAD_LEN 32
#define FMC_OPTION_COUNT 6

typedef enum FMCPage {
    FMC_PAGE_MENU = 0,
    FMC_PAGE_STATUS = 1,
    FMC_PAGE_RTE = 2,
    FMC_PAGE_CLB = 3,
    FMC_PAGE_CRZ = 4,
    FMC_PAGE_DES = 5,
    FMC_PAGE_DEP_ARR = 6
} FMCPage;

typedef struct FMCAirportRecord {
    char code[FMC_AIRPORT_CODE_LEN];
    double latitude;
    double longitude;
} FMCAirportRecord;

typedef struct FMCFixRecord {
    char name[FMC_FIX_NAME_LEN];
    double latitude;
    double longitude;
} FMCFixRecord;

typedef struct FMCRouteEntry {
    char name[FMC_FIX_NAME_LEN];
    double latitude;
    double longitude;
    char kind[8];
} FMCRouteEntry;

typedef struct FMCPhaseData {
    int speed;
    int altitude;
    int transition_altitude;
    int vref;
} FMCPhaseData;

typedef struct FMCProcedureOptions {
    char dep_runways[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    char dep_sids[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    char dep_transitions[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    char arr_runways[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    char arr_stars[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    char arr_transitions[FMC_OPTION_COUNT][FMC_FIX_NAME_LEN];
    int dep_runway_count;
    int dep_sid_count;
    int dep_transition_count;
    int arr_runway_count;
    int arr_star_count;
    int arr_transition_count;
    int dep_runway_index;
    int dep_sid_index;
    int dep_transition_index;
    int arr_runway_index;
    int arr_star_index;
    int arr_transition_index;
} FMCProcedureOptions;

typedef struct FMCAirportNode FMCAirportNode;
typedef struct FMCFixNode FMCFixNode;

extern FMCPage fmc_current_page;
extern int fmc_route_page;
extern int fmc_dep_arr_page;
extern int fmc_exec_armed;
extern char fmc_scratchpad[FMC_SCRATCHPAD_LEN];
extern char fmc_status_message[64];
extern char fmc_origin[FMC_AIRPORT_CODE_LEN];
extern char fmc_destination[FMC_AIRPORT_CODE_LEN];
extern char fmc_pending_origin[FMC_AIRPORT_CODE_LEN];
extern char fmc_pending_destination[FMC_AIRPORT_CODE_LEN];
extern FMCRouteEntry fmc_route[FMC_ROUTE_CAPACITY];
extern size_t fmc_route_count;
extern FMCPhaseData fmc_climb_data;
extern FMCPhaseData fmc_cruise_data;
extern FMCPhaseData fmc_descent_data;
extern FMCProcedureOptions fmc_proc_options;
extern int fmc_airport_count;
extern int fmc_fix_count;

void fmc_init_state(void);
int fmc_load_navigation_data(const char *apt_path, const char *fix_path, const char *fms_path);
void fmc_free_navigation_data(void);
const FMCAirportRecord *fmc_find_airport(const char *code);
const FMCFixRecord *fmc_find_fix(const char *name);
int fmc_stage_airport(int is_origin, const char *code);
int fmc_commit_exec(void);
int fmc_append_route_waypoint(const char *name);
void fmc_clear_scratchpad(void);
void fmc_backspace_scratchpad(void);
void fmc_append_scratchpad_text(const char *text);
void fmc_cycle_dep_arr_option(int slot, int direction);
void fmc_set_page(FMCPage page);

#endif

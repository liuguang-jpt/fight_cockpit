#ifndef EICAS1_DATA_H
#define EICAS1_DATA_H

#include "../Util/xplaneConnect.h"
#include <stdio.h>
#include <windows.h>

#define EICAS1_DATA_FILE_PATH "assets/assets/assets/eicas1.dat"

typedef enum EICAS1DataSource {
    EICAS1_SOURCE_STATIC = 0,
    EICAS1_SOURCE_FILE = 1,
    EICAS1_SOURCE_XPLANE = 2
} EICAS1DataSource;

typedef enum EICAS1DataMode {
    EICAS1_MODE_AUTO = 0,
    EICAS1_MODE_FILE = 1,
    EICAS1_MODE_XPLANE = 2
} EICAS1DataMode;

typedef struct EICAS1Data {
    float tat;
    float n1_left;
    float n1_right;
    float egt_left;
    float egt_right;
    float ff_left;
    float ff_right;
    float fuel_center;
    float fuel_left;
    float fuel_right;
    EICAS1DataSource source;
    int xplane_connected;
} EICAS1Data;

typedef struct EICAS1ThreadContext {
    EICAS1DataMode mode;
    unsigned short xp_port;
    const char *file_path;
} EICAS1ThreadContext;

extern volatile LONG g_eicas1_data_ready;
extern volatile LONG g_eicas1_exit_requested;
extern volatile LONG g_eicas1_thread_running;
extern EICAS1Data g_shared_eicas1_data;

extern float eicas1_tat;
extern float eicas1_n1_left;
extern float eicas1_n1_right;
extern float eicas1_egt_left;
extern float eicas1_egt_right;
extern float eicas1_ff_left;
extern float eicas1_ff_right;
extern float eicas1_fuel_center;
extern float eicas1_fuel_left;
extern float eicas1_fuel_right;
extern int eicas1_xplane_connected;
extern EICAS1DataSource eicas1_data_source;

void init_eicas1_defaults(EICAS1Data *data);
void sync_globals_from_eicas1_data(const EICAS1Data *data);
FILE *open_eicas1_data_file(const char *path);
int read_eicas1_data(FILE *file, EICAS1Data *data);
int load_next_eicas1_frame(FILE *file, EICAS1Data *data);
EICAS1DataMode get_eicas1_data_mode_from_env(void);
unsigned short get_eicas1_xplane_port_from_env(void);
int getEICAS1Data(XPCSocket sock, EICAS1Data *data);
DWORD WINAPI eicas1_data_thread_proc(LPVOID param);

#endif

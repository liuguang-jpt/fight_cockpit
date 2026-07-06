#ifndef EICAS2_DATA_H
#define EICAS2_DATA_H

#include "../Util/xplaneConnect.h"
#include <stdio.h>
#include <windows.h>

#define EICAS2_DATA_FILE_PATH "assets/assets/assets/eicas2.dat"

typedef enum EICAS2DataSource {
    EICAS2_SOURCE_STATIC = 0,
    EICAS2_SOURCE_FILE = 1,
    EICAS2_SOURCE_XPLANE = 2
} EICAS2DataSource;

typedef enum EICAS2DataMode {
    EICAS2_MODE_AUTO = 0,
    EICAS2_MODE_FILE = 1,
    EICAS2_MODE_XPLANE = 2
} EICAS2DataMode;

typedef struct EICAS2Data {
    float n2_left;
    float n2_right;
    float vib_left;
    float vib_right;
    float oil_pressure_left;
    float oil_pressure_right;
    float oil_temp_left;
    float oil_temp_right;
    float oil_qty_left;
    float oil_qty_right;
    float fuel_flow_left;
    float fuel_flow_right;
    EICAS2DataSource source;
    int xplane_connected;
} EICAS2Data;

typedef struct EICAS2ThreadContext {
    EICAS2DataMode mode;
    unsigned short xp_port;
    const char *file_path;
} EICAS2ThreadContext;

extern volatile LONG g_eicas2_data_ready;
extern volatile LONG g_eicas2_exit_requested;
extern volatile LONG g_eicas2_thread_running;
extern EICAS2Data g_shared_eicas2_data;

extern float eicas2_n2_left;
extern float eicas2_n2_right;
extern float eicas2_vib_left;
extern float eicas2_vib_right;
extern float eicas2_oil_pressure_left;
extern float eicas2_oil_pressure_right;
extern float eicas2_oil_temp_left;
extern float eicas2_oil_temp_right;
extern float eicas2_oil_qty_left;
extern float eicas2_oil_qty_right;
extern float eicas2_fuel_flow_left;
extern float eicas2_fuel_flow_right;
extern int eicas2_xplane_connected;
extern EICAS2DataSource eicas2_data_source;

void init_eicas2_defaults(EICAS2Data *data);
void sync_globals_from_eicas2_data(const EICAS2Data *data);
FILE *open_eicas2_data_file(const char *path);
int read_eicas2_data(FILE *file, EICAS2Data *data);
int load_next_eicas2_frame(FILE *file, EICAS2Data *data);
EICAS2DataMode get_eicas2_data_mode_from_env(void);
unsigned short get_eicas2_xplane_port_from_env(void);
int getEICAS2Data(XPCSocket sock, EICAS2Data *data);
DWORD WINAPI eicas2_data_thread_proc(LPVOID param);

#endif

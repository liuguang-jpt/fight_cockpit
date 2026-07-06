#ifndef PFD_DATA_H
#define PFD_DATA_H

#include "../Util/xplaneConnect.h"
#include <stdio.h>
#include <windows.h>

#define PFD_DATA_FILE_PATH "assets/assets/assets/pfd.dat"

#define AIRSPEED_MIN 0
#define AIRSPEED_MAX 440
#define DISPLAY_RANGE 100

#define ALTITUDE_MIN -1000
#define ALTITUDE_MAX 50000
#define ALTITUDE_TICK_INTERVAL 100
#define ALTITUDE_LABEL_INTERVAL 200
#define ALTITUDE_DISPLAY_RANGE 600

#define VERTICAL_SPEED_MIN -6000
#define VERTICAL_SPEED_MAX 6000
#define VERTICAL_SPEED_TEXT_MAX 9999

#define THRUST_MIN 0
#define THRUST_MAX 100

typedef enum PFDDataSource {
    PFD_SOURCE_STATIC = 0,
    PFD_SOURCE_FILE = 1,
    PFD_SOURCE_XPLANE = 2
} PFDDataSource;

typedef enum PFDDataMode {
    PFD_MODE_AUTO = 0,
    PFD_MODE_FILE = 1,
    PFD_MODE_XPLANE = 2
} PFDDataMode;

typedef struct PFDData {
    int current_airspeed;
    int target_airspeed;
    int current_altitude;
    int target_altitude;
    int agl_altitude;
    float current_pitch;
    float current_roll;
    int current_vertical_speed;
    int current_heading;
    int target_heading;
    int current_thrust;
    PFDDataSource source;
    int xplane_connected;
} PFDData;

typedef struct PFDThreadContext {
    PFDDataMode mode;
    unsigned short xp_port;
    const char *file_path;
} PFDThreadContext;

extern volatile LONG g_pfd_data_ready;
extern volatile LONG g_pfd_exit_requested;
extern volatile LONG g_pfd_thread_running;
extern PFDData g_shared_pfd_data;

extern int current_airspeed;
extern int target_airspeed;
extern int current_altitude;
extern int target_altitude;
extern int agl_altitude;
extern float current_pitch;
extern float current_roll;
extern int current_vertical_speed;
extern int current_heading;
extern int target_heading;
extern int current_thrust;
extern int current_xplane_connected;
extern PFDDataSource current_data_source;

void init_pfd_defaults(PFDData *data);
void sync_globals_from_pfd_data(const PFDData *data);
FILE *open_pfd_data_file(const char *path);
int read_pfd_data(FILE *file, PFDData *data);
int load_next_file_frame(FILE *file, PFDData *data);
PFDDataMode get_pfd_data_mode_from_env(void);
unsigned short get_xplane_port_from_env(void);
int getPFDData(XPCSocket sock, PFDData *data);
DWORD WINAPI pfd_data_thread_proc(LPVOID param);

#endif

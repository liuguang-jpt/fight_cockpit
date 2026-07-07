#ifndef ND_DATA_H
#define ND_DATA_H

#include "../Util/xplaneConnect.h"
#include <stdio.h>
#include <windows.h>

#define ND_DATA_FILE_PATH "assets/assets/assets/nd.dat"
#define ND_ROUTE_POINT_COUNT 6

/* ND runs in map mode for now, with file and X-Plane data sources sharing one snapshot structure. */
typedef enum NDDataSource {
    ND_SOURCE_STATIC = 0,
    ND_SOURCE_FILE = 1,
    ND_SOURCE_XPLANE = 2
} NDDataSource;

typedef enum NDDataMode {
    ND_MODE_AUTO = 0,
    ND_MODE_FILE = 1,
    ND_MODE_XPLANE = 2
} NDDataMode;

typedef struct NDRoutePoint {
    char name[8];
    float bearing_deg;
    float distance_nm;
    int is_active;
} NDRoutePoint;

typedef struct NDData {
    int ground_speed;
    int true_airspeed;
    int current_heading;
    int current_track;
    int wind_direction;
    int wind_speed;
    float latitude_deg;
    float longitude_deg;
    NDRoutePoint route_points[ND_ROUTE_POINT_COUNT];
    NDDataSource source;
    int xplane_connected;
    int fmc_link_active;
} NDData;

typedef struct NDThreadContext {
    NDDataMode mode;
    unsigned short xp_port;
    const char *file_path;
} NDThreadContext;

extern volatile LONG g_nd_data_ready;
extern volatile LONG g_nd_exit_requested;
extern volatile LONG g_nd_thread_running;
extern NDData g_shared_nd_data;

extern int nd_ground_speed;
extern int nd_true_airspeed;
extern int nd_current_heading;
extern int nd_current_track;
extern int nd_wind_direction;
extern int nd_wind_speed;
extern float nd_latitude_deg;
extern float nd_longitude_deg;
extern NDRoutePoint nd_route_points[ND_ROUTE_POINT_COUNT];
extern int nd_xplane_connected;
extern int nd_fmc_link_active;
extern NDDataSource nd_data_source;

void init_nd_defaults(NDData *data);
void sync_globals_from_nd_data(const NDData *data);
void nd_apply_fmc_route(NDData *data);
FILE *open_nd_data_file(const char *path);
int read_nd_data(FILE *file, NDData *data);
int load_next_nd_frame(FILE *file, NDData *data);
NDDataMode get_nd_data_mode_from_env(void);
unsigned short get_nd_xplane_port_from_env(void);
int getNDData(XPCSocket sock, NDData *data);
DWORD WINAPI nd_data_thread_proc(LPVOID param);

#endif

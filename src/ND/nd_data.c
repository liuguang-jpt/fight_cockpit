#include "nd_data.h"
#include "../Util/fmc_nd_link.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

volatile LONG g_nd_data_ready = 0;
volatile LONG g_nd_exit_requested = 0;
volatile LONG g_nd_thread_running = 0;
NDData g_shared_nd_data;

int nd_ground_speed = 260, nd_true_airspeed = 260;
int nd_current_heading = 260, nd_current_track = 260;
int nd_wind_direction = 0, nd_wind_speed = 25;
float nd_latitude_deg = 31.8f, nd_longitude_deg = 117.2f;
NDRoutePoint nd_route_points[ND_ROUTE_POINT_COUNT];
int nd_xplane_connected = 0;
int nd_fmc_link_active = 0;
NDDataSource nd_data_source = ND_SOURCE_STATIC;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int normalize_heading(int value) {
    int heading = value % 360;
    if (heading < 0) heading += 360;
    return heading;
}

static int round_to_int(float value) {
    return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static float degrees_to_radians(float degrees) {
    return degrees * 0.01745329252f;
}

static float calculate_bearing_deg(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = degrees_to_radians(lat1);
    float phi2 = degrees_to_radians(lat2);
    float delta_lambda = degrees_to_radians(lon2 - lon1);
    float y = sinf(delta_lambda) * cosf(phi2);
    float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(delta_lambda);
    float bearing = atan2f(y, x) * 57.2957795f;

    if (bearing < 0.0f) bearing += 360.0f;
    return bearing;
}

static float calculate_distance_nm(float lat1, float lon1, float lat2, float lon2) {
    float dlat = degrees_to_radians(lat2 - lat1);
    float dlon = degrees_to_radians(lon2 - lon1);
    float phi1 = degrees_to_radians(lat1);
    float phi2 = degrees_to_radians(lat2);
    float sin_dlat = sinf(dlat * 0.5f);
    float sin_dlon = sinf(dlon * 0.5f);
    float a = sin_dlat * sin_dlat + cosf(phi1) * cosf(phi2) * sin_dlon * sin_dlon;
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    return 3440.065f * c;
}

static void assign_default_route(NDData *data) {
    static const NDRoutePoint defaults[ND_ROUTE_POINT_COUNT] = {
        {"TOC", 315.0f, 14.0f, 1}, {"B221", 338.0f, 29.0f, 1},
        {"WPT3", 2.0f, 45.0f, 1}, {"DADON", 18.0f, 62.0f, 1},
        {"VOR1", 294.0f, 38.0f, 1}, {"APT", 22.0f, 76.0f, 1}
    };
    memcpy(data->route_points, defaults, sizeof(defaults));
}

/* ND only renders a handful of waypoints, so the FMC route is projected into the visible 6-point window here. */
void nd_apply_fmc_route(NDData *data) {
    FmcNdSharedRoute shared_route;
    size_t start_index = 0;
    size_t i;

    if (data == NULL) return;
    if (!fmc_nd_link_read(&shared_route) || shared_route.route_count == 0) {
        data->fmc_link_active = 0;
        return;
    }

    data->fmc_link_active = 1;
    if (data->source != ND_SOURCE_XPLANE && shared_route.route_count > 0) {
        data->latitude_deg = (float)shared_route.route[0].latitude;
        data->longitude_deg = (float)shared_route.route[0].longitude;
        if (shared_route.route_count > 1) {
            int route_heading = normalize_heading(round_to_int(calculate_bearing_deg(
                data->latitude_deg,
                data->longitude_deg,
                (float)shared_route.route[1].latitude,
                (float)shared_route.route[1].longitude
            )));
            data->current_heading = route_heading;
            data->current_track = route_heading;
        }
    }

    memset(data->route_points, 0, sizeof(data->route_points));
    start_index = shared_route.route_count > 1 ? 1 : 0;
    for (i = 0; i < ND_ROUTE_POINT_COUNT && start_index + i < shared_route.route_count; ++i) {
        FmcNdLinkRouteItem *item = &shared_route.route[start_index + i];

        snprintf(data->route_points[i].name, sizeof(data->route_points[i].name), "%s", item->name);
        data->route_points[i].bearing_deg = calculate_bearing_deg(
            data->latitude_deg,
            data->longitude_deg,
            (float)item->latitude,
            (float)item->longitude
        );
        data->route_points[i].distance_nm = calculate_distance_nm(
            data->latitude_deg,
            data->longitude_deg,
            (float)item->latitude,
            (float)item->longitude
        );
        data->route_points[i].is_active = 1;
    }
}

void init_nd_defaults(NDData *data) {
    if (data == NULL) return;
    data->ground_speed = 260;
    data->true_airspeed = 260;
    data->current_heading = 260;
    data->current_track = 260;
    data->wind_direction = 0;
    data->wind_speed = 25;
    data->latitude_deg = 31.8f;
    data->longitude_deg = 117.2f;
    data->source = ND_SOURCE_STATIC;
    data->xplane_connected = 0;
    data->fmc_link_active = 0;
    assign_default_route(data);
}

void sync_globals_from_nd_data(const NDData *data) {
    if (data == NULL) return;
    nd_ground_speed = data->ground_speed;
    nd_true_airspeed = data->true_airspeed;
    nd_current_heading = data->current_heading;
    nd_current_track = data->current_track;
    nd_wind_direction = data->wind_direction;
    nd_wind_speed = data->wind_speed;
    nd_latitude_deg = data->latitude_deg;
    nd_longitude_deg = data->longitude_deg;
    memcpy(nd_route_points, data->route_points, sizeof(nd_route_points));
    nd_xplane_connected = data->xplane_connected;
    nd_fmc_link_active = data->fmc_link_active;
    nd_data_source = data->source;
}

FILE *open_nd_data_file(const char *path) {
    const char *final_path = path == NULL ? ND_DATA_FILE_PATH : path;
    return fopen(final_path, "r");
}

int read_nd_data(FILE *file, NDData *data) {
    char line[256];
    int fields[4];

    if (file == NULL || data == NULL) return 0;
    if (fgets(line, sizeof(line), file) == NULL) return 0;
    if (sscanf(line, "%d,%d,%d,%d", &fields[0], &fields[1], &fields[2], &fields[3]) != 4) return 0;

    data->ground_speed = clamp_int(fields[0], 0, 999);
    data->true_airspeed = clamp_int(fields[1], 0, 999);
    data->current_heading = normalize_heading(fields[0]);
    data->current_track = normalize_heading(fields[1]);
    data->wind_direction = normalize_heading(fields[2]);
    data->wind_speed = clamp_int(fields[3], 0, 200);

    /* File mode only carries a few fields, so derive a small moving map position from heading/track. */
    data->latitude_deg = 31.8f + sinf((float)data->current_heading * 0.0174533f) * 0.15f;
    data->longitude_deg = 117.2f + cosf((float)data->current_track * 0.0174533f) * 0.15f;
    data->source = ND_SOURCE_FILE;
    data->xplane_connected = 0;
    data->fmc_link_active = 0;
    assign_default_route(data);
    nd_apply_fmc_route(data);
    return 1;
}

int load_next_nd_frame(FILE *file, NDData *data) {
    if (read_nd_data(file, data)) return 1;
    if (file == NULL) return 0;
    fseek(file, 0, SEEK_SET);
    return read_nd_data(file, data);
}

NDDataMode get_nd_data_mode_from_env(void) {
    const char *mode = getenv("ND_DATA_MODE");
    if (mode == NULL || *mode == '\0') return ND_MODE_AUTO;
    if (_stricmp(mode, "file") == 0) return ND_MODE_FILE;
    if (_stricmp(mode, "xplane") == 0) return ND_MODE_XPLANE;
    return ND_MODE_AUTO;
}

unsigned short get_nd_xplane_port_from_env(void) {
    const char *port_text = getenv("ND_XPLANE_PORT");
    long port = 0;

    if (port_text == NULL || *port_text == '\0') return XPC_DEFAULT_PORT;
    port = strtol(port_text, NULL, 10);
    if (port <= 0 || port > 65535) return XPC_DEFAULT_PORT;
    return (unsigned short)port;
}

int getNDData(XPCSocket sock, NDData *data) {
    /* Fetch all visible ND fields in one UDP round-trip so one frame uses one coherent data snapshot. */
    const char *drefs[] = {
        "sim/cockpit2/gauges/indicators/ground_speed_kt",
        "sim/cockpit2/gauges/indicators/true_airspeed_kts_pilot",
        "sim/cockpit2/gauges/indicators/heading_AHARS_deg_mag_pilot",
        "sim/cockpit2/gauges/indicators/ground_track_mag_pilot",
        "sim/cockpit2/gauges/indicators/wind_heading_deg_mag",
        "sim/cockpit2/gauges/indicators/wind_speed_kts",
        "sim/flightmodel/position/latitude",
        "sim/flightmodel/position/longitude"
    };
    float ground_speed = 0.0f, true_airspeed = 0.0f, heading = 0.0f, track = 0.0f;
    float wind_direction = 0.0f, wind_speed = 0.0f, latitude = 0.0f, longitude = 0.0f;
    float *values[] = {
        &ground_speed, &true_airspeed, &heading, &track,
        &wind_direction, &wind_speed, &latitude, &longitude
    };
    int sizes[] = {1, 1, 1, 1, 1, 1, 1, 1};

    if (data == NULL) return -1;
    if (getDREFs(sock, drefs, values, (unsigned char)(sizeof(values) / sizeof(values[0])), sizes) != 0) {
        data->xplane_connected = 0;
        return -2;
    }

    data->ground_speed = clamp_int(round_to_int(ground_speed), 0, 999);
    data->true_airspeed = clamp_int(round_to_int(true_airspeed), 0, 999);
    data->current_heading = normalize_heading(round_to_int(heading));
    data->current_track = normalize_heading(round_to_int(track));
    data->wind_direction = normalize_heading(round_to_int(wind_direction));
    data->wind_speed = clamp_int(round_to_int(wind_speed), 0, 200);
    data->latitude_deg = latitude;
    data->longitude_deg = longitude;
    data->source = ND_SOURCE_XPLANE;
    data->xplane_connected = 1;
    data->fmc_link_active = 0;
    assign_default_route(data);
    nd_apply_fmc_route(data);
    return 0;
}

DWORD WINAPI nd_data_thread_proc(LPVOID param) {
    NDThreadContext *ctx = (NDThreadContext *)param;
    NDData local_data;
    FILE *file = NULL;
    XPCSocket sock;
    int file_backoff = 0;

    init_nd_defaults(&local_data);
    sock.sock = INVALID_SOCKET;
    InterlockedExchange(&g_nd_thread_running, 1);

    if (ctx != NULL && ctx->mode != ND_MODE_XPLANE) file = open_nd_data_file(ctx->file_path);
    if (ctx != NULL && ctx->mode != ND_MODE_FILE) sock = openUDP(ctx->xp_port);

    while (InterlockedCompareExchange(&g_nd_exit_requested, 0, 0) == 0) {
        int has_update = 0;

        if (ctx != NULL && ctx->mode != ND_MODE_FILE && sock.sock != INVALID_SOCKET) {
            if (getNDData(sock, &local_data) == 0) {
                has_update = 1;
                Sleep(500);
            } else if (ctx->mode == ND_MODE_XPLANE) {
                local_data.source = ND_SOURCE_STATIC;
                local_data.xplane_connected = 0;
                local_data.fmc_link_active = 0;
                nd_apply_fmc_route(&local_data);
                Sleep(250);
                has_update = 1;
            }
        }

        if (!has_update && ctx != NULL && ctx->mode != ND_MODE_XPLANE) {
            if (file == NULL && file_backoff == 0) {
                file = open_nd_data_file(ctx->file_path);
                if (file == NULL) file_backoff = 30;
            }

            if (file != NULL && load_next_nd_frame(file, &local_data)) {
                has_update = 1;
                Sleep(16);
            } else {
                if (file != NULL) {
                    fclose(file);
                    file = NULL;
                }
                if (file_backoff > 0) --file_backoff;
                Sleep(100);
            }
        }

        if (!has_update) {
            local_data.source = ND_SOURCE_STATIC;
            local_data.xplane_connected = 0;
            local_data.fmc_link_active = 0;
            assign_default_route(&local_data);
            nd_apply_fmc_route(&local_data);
            Sleep(100);
            has_update = 1;
        }

        if (has_update && InterlockedCompareExchange(&g_nd_data_ready, 0, 0) == 0) {
            /* The worker only publishes a new snapshot after the UI thread has consumed the previous one. */
            g_shared_nd_data = local_data;
            InterlockedExchange(&g_nd_data_ready, 1);
        }
    }

    if (file != NULL) fclose(file);
    if (sock.sock != INVALID_SOCKET) closeUDP(sock);
    fmc_nd_link_shutdown();
    InterlockedExchange(&g_nd_thread_running, 0);
    return 0;
}

#include "eicas1_data.h"

#include <math.h>
#include <stdlib.h>

volatile LONG g_eicas1_data_ready = 0;
volatile LONG g_eicas1_exit_requested = 0;
volatile LONG g_eicas1_thread_running = 0;
EICAS1Data g_shared_eicas1_data;

float eicas1_tat = 10.0f;
float eicas1_n1_left = 60.0f;
float eicas1_n1_right = 63.5f;
float eicas1_egt_left = 650.0f;
float eicas1_egt_right = 660.0f;
float eicas1_ff_left = 8.84f;
float eicas1_ff_right = 1.02f;
float eicas1_fuel_center = 5000.0f;
float eicas1_fuel_left = 4000.0f;
float eicas1_fuel_right = 4000.0f;
int eicas1_xplane_connected = 0;
EICAS1DataSource eicas1_data_source = EICAS1_SOURCE_STATIC;

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void init_eicas1_defaults(EICAS1Data *data) {
    if (data == NULL) return;
    data->tat = 10.0f;
    data->n1_left = 60.0f;
    data->n1_right = 63.5f;
    data->egt_left = 650.0f;
    data->egt_right = 660.0f;
    data->ff_left = 8.84f;
    data->ff_right = 1.02f;
    data->fuel_center = 5000.0f;
    data->fuel_left = 4000.0f;
    data->fuel_right = 4000.0f;
    data->source = EICAS1_SOURCE_STATIC;
    data->xplane_connected = 0;
}

void sync_globals_from_eicas1_data(const EICAS1Data *data) {
    if (data == NULL) return;
    eicas1_tat = data->tat;
    eicas1_n1_left = data->n1_left;
    eicas1_n1_right = data->n1_right;
    eicas1_egt_left = data->egt_left;
    eicas1_egt_right = data->egt_right;
    eicas1_ff_left = data->ff_left;
    eicas1_ff_right = data->ff_right;
    eicas1_fuel_center = data->fuel_center;
    eicas1_fuel_left = data->fuel_left;
    eicas1_fuel_right = data->fuel_right;
    eicas1_xplane_connected = data->xplane_connected;
    eicas1_data_source = data->source;
}

FILE *open_eicas1_data_file(const char *path) {
    const char *final_path = path == NULL ? EICAS1_DATA_FILE_PATH : path;
    return fopen(final_path, "r");
}

int read_eicas1_data(FILE *file, EICAS1Data *data) {
    char line[256];
    float values[10];

    if (file == NULL || data == NULL) return 0;
    if (fgets(line, sizeof(line), file) == NULL) return 0;
    if (sscanf(line, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
        &values[0], &values[1], &values[2], &values[3], &values[4],
        &values[5], &values[6], &values[7], &values[8], &values[9]) != 10) return 0;

    data->tat = values[0];
    data->n1_left = clamp_float(values[1], 0.0f, 110.0f);
    data->n1_right = clamp_float(values[2], 0.0f, 110.0f);
    data->egt_left = clamp_float(values[3], 0.0f, 1200.0f);
    data->egt_right = clamp_float(values[4], 0.0f, 1200.0f);
    data->ff_left = clamp_float(values[5], 0.0f, 50.0f);
    data->ff_right = clamp_float(values[6], 0.0f, 50.0f);
    data->fuel_center = clamp_float(values[7], 0.0f, 99999.0f);
    data->fuel_left = clamp_float(values[8], 0.0f, 99999.0f);
    data->fuel_right = clamp_float(values[9], 0.0f, 99999.0f);
    data->source = EICAS1_SOURCE_FILE;
    data->xplane_connected = 0;
    return 1;
}

int load_next_eicas1_frame(FILE *file, EICAS1Data *data) {
    if (read_eicas1_data(file, data)) return 1;
    if (file == NULL) return 0;
    fseek(file, 0, SEEK_SET);
    return read_eicas1_data(file, data);
}

EICAS1DataMode get_eicas1_data_mode_from_env(void) {
    const char *mode = getenv("EICAS1_DATA_MODE");
    if (mode == NULL || *mode == '\0') return EICAS1_MODE_AUTO;
    if (_stricmp(mode, "file") == 0) return EICAS1_MODE_FILE;
    if (_stricmp(mode, "xplane") == 0) return EICAS1_MODE_XPLANE;
    return EICAS1_MODE_AUTO;
}

unsigned short get_eicas1_xplane_port_from_env(void) {
    const char *port_text = getenv("EICAS1_XPLANE_PORT");
    long port = 0;
    if (port_text == NULL || *port_text == '\0') return XPC_DEFAULT_PORT;
    port = strtol(port_text, NULL, 10);
    if (port <= 0 || port > 65535) return XPC_DEFAULT_PORT;
    return (unsigned short)port;
}

int getEICAS1Data(XPCSocket sock, EICAS1Data *data) {
    const char *drefs[] = {
        "sim/cockpit2/temperature/outside_air_temp_degc",
        "sim/cockpit2/engine/indicators/N1_percent",
        "sim/cockpit2/engine/indicators/EGT_deg_C",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec",
        "sim/cockpit2/fuel/fuel_quantity"
    };
    float tat = 0.0f;
    float n1[8] = {0.0f};
    float egt[8] = {0.0f};
    float ff[8] = {0.0f};
    float fuel[9] = {0.0f};
    float *values[] = {&tat, n1, egt, ff, fuel};
    int sizes[] = {1, 8, 8, 8, 9};

    if (data == NULL) return -1;
    if (getDREFs(sock, drefs, values, (unsigned char)(sizeof(values) / sizeof(values[0])), sizes) != 0) {
        data->xplane_connected = 0;
        return -2;
    }

    data->tat = tat;
    data->n1_left = clamp_float(n1[0], 0.0f, 110.0f);
    data->n1_right = clamp_float(n1[1], 0.0f, 110.0f);
    data->egt_left = clamp_float(egt[0], 0.0f, 1200.0f);
    data->egt_right = clamp_float(egt[1], 0.0f, 1200.0f);
    data->ff_left = clamp_float(ff[0] * 3600.0f, 0.0f, 999.0f);
    data->ff_right = clamp_float(ff[1] * 3600.0f, 0.0f, 999.0f);
    data->fuel_center = clamp_float(fuel[0], 0.0f, 99999.0f);
    data->fuel_left = clamp_float(fuel[1], 0.0f, 99999.0f);
    data->fuel_right = clamp_float(fuel[2], 0.0f, 99999.0f);
    data->source = EICAS1_SOURCE_XPLANE;
    data->xplane_connected = 1;
    return 0;
}

DWORD WINAPI eicas1_data_thread_proc(LPVOID param) {
    EICAS1ThreadContext *ctx = (EICAS1ThreadContext *)param;
    EICAS1Data local_data;
    FILE *file = NULL;
    XPCSocket sock;
    int file_backoff = 0;

    init_eicas1_defaults(&local_data);
    sock.sock = INVALID_SOCKET;
    InterlockedExchange(&g_eicas1_thread_running, 1);
    if (ctx != NULL && ctx->mode != EICAS1_MODE_XPLANE) file = open_eicas1_data_file(ctx->file_path);
    if (ctx != NULL && ctx->mode != EICAS1_MODE_FILE) sock = openUDP(ctx->xp_port);

    while (InterlockedCompareExchange(&g_eicas1_exit_requested, 0, 0) == 0) {
        int has_update = 0;

        if (ctx != NULL && ctx->mode != EICAS1_MODE_FILE && sock.sock != INVALID_SOCKET) {
            if (getEICAS1Data(sock, &local_data) == 0) {
                has_update = 1;
                Sleep(500);
            } else if (ctx->mode == EICAS1_MODE_XPLANE) {
                local_data.source = EICAS1_SOURCE_STATIC;
                local_data.xplane_connected = 0;
                Sleep(250);
                has_update = 1;
            }
        }

        if (!has_update && ctx != NULL && ctx->mode != EICAS1_MODE_XPLANE) {
            if (file == NULL && file_backoff == 0) {
                file = open_eicas1_data_file(ctx->file_path);
                if (file == NULL) file_backoff = 30;
            }

            if (file != NULL && load_next_eicas1_frame(file, &local_data)) {
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
            local_data.source = EICAS1_SOURCE_STATIC;
            local_data.xplane_connected = 0;
            Sleep(100);
            has_update = 1;
        }

        if (has_update && InterlockedCompareExchange(&g_eicas1_data_ready, 0, 0) == 0) {
            g_shared_eicas1_data = local_data;
            InterlockedExchange(&g_eicas1_data_ready, 1);
        }
    }

    if (file != NULL) fclose(file);
    if (sock.sock != INVALID_SOCKET) closeUDP(sock);
    InterlockedExchange(&g_eicas1_thread_running, 0);
    return 0;
}

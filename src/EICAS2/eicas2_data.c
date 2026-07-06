#include "eicas2_data.h"

#include <math.h>
#include <stdlib.h>

volatile LONG g_eicas2_data_ready = 0;
volatile LONG g_eicas2_exit_requested = 0;
volatile LONG g_eicas2_thread_running = 0;
EICAS2Data g_shared_eicas2_data;

float eicas2_n2_left = 70.0f, eicas2_n2_right = 72.5f;
float eicas2_vib_left = 3.54f, eicas2_vib_right = 4.85f;
float eicas2_oil_pressure_left = 40.0f, eicas2_oil_pressure_right = 40.7f;
float eicas2_oil_temp_left = 90.0f, eicas2_oil_temp_right = 91.2f;
float eicas2_oil_qty_left = 12.0f, eicas2_oil_qty_right = 12.0f;
float eicas2_fuel_flow_left = 1.0f, eicas2_fuel_flow_right = 1.04f;
int eicas2_xplane_connected = 0;
EICAS2DataSource eicas2_data_source = EICAS2_SOURCE_STATIC;

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void init_eicas2_defaults(EICAS2Data *data) {
    if (data == NULL) return;
    data->n2_left = 70.0f;
    data->n2_right = 72.5f;
    data->vib_left = 3.54f;
    data->vib_right = 4.85f;
    data->oil_pressure_left = 40.0f;
    data->oil_pressure_right = 40.7f;
    data->oil_temp_left = 90.0f;
    data->oil_temp_right = 91.2f;
    data->oil_qty_left = 12.0f;
    data->oil_qty_right = 12.0f;
    data->fuel_flow_left = 1.0f;
    data->fuel_flow_right = 1.04f;
    data->source = EICAS2_SOURCE_STATIC;
    data->xplane_connected = 0;
}

void sync_globals_from_eicas2_data(const EICAS2Data *data) {
    if (data == NULL) return;
    eicas2_n2_left = data->n2_left;
    eicas2_n2_right = data->n2_right;
    eicas2_vib_left = data->vib_left;
    eicas2_vib_right = data->vib_right;
    eicas2_oil_pressure_left = data->oil_pressure_left;
    eicas2_oil_pressure_right = data->oil_pressure_right;
    eicas2_oil_temp_left = data->oil_temp_left;
    eicas2_oil_temp_right = data->oil_temp_right;
    eicas2_oil_qty_left = data->oil_qty_left;
    eicas2_oil_qty_right = data->oil_qty_right;
    eicas2_fuel_flow_left = data->fuel_flow_left;
    eicas2_fuel_flow_right = data->fuel_flow_right;
    eicas2_xplane_connected = data->xplane_connected;
    eicas2_data_source = data->source;
}

FILE *open_eicas2_data_file(const char *path) {
    const char *final_path = path == NULL ? EICAS2_DATA_FILE_PATH : path;
    return fopen(final_path, "r");
}

int read_eicas2_data(FILE *file, EICAS2Data *data) {
    char line[256];
    float values[12];

    if (file == NULL || data == NULL) return 0;
    if (fgets(line, sizeof(line), file) == NULL) return 0;
    if (sscanf(line, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
        &values[0], &values[1], &values[2], &values[3], &values[4], &values[5],
        &values[6], &values[7], &values[8], &values[9], &values[10], &values[11]) != 12) return 0;

    data->n2_left = clamp_float(values[0], 0.0f, 110.0f);
    data->n2_right = clamp_float(values[1], 0.0f, 110.0f);
    data->vib_left = clamp_float(values[2], 0.0f, 15.0f);
    data->vib_right = clamp_float(values[3], 0.0f, 15.0f);
    data->oil_pressure_left = clamp_float(values[4], 0.0f, 120.0f);
    data->oil_pressure_right = clamp_float(values[5], 0.0f, 120.0f);
    data->oil_temp_left = clamp_float(values[6], 0.0f, 200.0f);
    data->oil_temp_right = clamp_float(values[7], 0.0f, 200.0f);
    data->oil_qty_left = clamp_float(values[8], 0.0f, 40.0f);
    data->oil_qty_right = clamp_float(values[9], 0.0f, 40.0f);
    data->fuel_flow_left = clamp_float(values[10], 0.0f, 50.0f);
    data->fuel_flow_right = clamp_float(values[11], 0.0f, 50.0f);
    data->source = EICAS2_SOURCE_FILE;
    data->xplane_connected = 0;
    return 1;
}

int load_next_eicas2_frame(FILE *file, EICAS2Data *data) {
    if (read_eicas2_data(file, data)) return 1;
    if (file == NULL) return 0;
    fseek(file, 0, SEEK_SET);
    return read_eicas2_data(file, data);
}

EICAS2DataMode get_eicas2_data_mode_from_env(void) {
    const char *mode = getenv("EICAS2_DATA_MODE");
    if (mode == NULL || *mode == '\0') return EICAS2_MODE_AUTO;
    if (_stricmp(mode, "file") == 0) return EICAS2_MODE_FILE;
    if (_stricmp(mode, "xplane") == 0) return EICAS2_MODE_XPLANE;
    return EICAS2_MODE_AUTO;
}

unsigned short get_eicas2_xplane_port_from_env(void) {
    const char *port_text = getenv("EICAS2_XPLANE_PORT");
    long port = 0;
    if (port_text == NULL || *port_text == '\0') return XPC_DEFAULT_PORT;
    port = strtol(port_text, NULL, 10);
    if (port <= 0 || port > 65535) return XPC_DEFAULT_PORT;
    return (unsigned short)port;
}

int getEICAS2Data(XPCSocket sock, EICAS2Data *data) {
    const char *drefs[] = {
        "sim/cockpit2/engine/indicators/N2_percent",
        "sim/cockpit2/engine/indicators/oil_pressure_psi",
        "sim/cockpit2/engine/indicators/oil_temperature_deg_C",
        "sim/cockpit2/engine/indicators/oil_quantity_ratio",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec"
    };
    float n2[8] = {0.0f};
    float oil_pressure[8] = {0.0f};
    float oil_temp[8] = {0.0f};
    float oil_qty[8] = {0.0f};
    float fuel_flow[8] = {0.0f};
    float *values[] = {n2, oil_pressure, oil_temp, oil_qty, fuel_flow};
    int sizes[] = {8, 8, 8, 8, 8};

    if (data == NULL) return -1;
    if (getDREFs(sock, drefs, values, (unsigned char)(sizeof(values) / sizeof(values[0])), sizes) != 0) {
        data->xplane_connected = 0;
        return -2;
    }

    data->n2_left = clamp_float(n2[0], 0.0f, 110.0f);
    data->n2_right = clamp_float(n2[1], 0.0f, 110.0f);
    data->vib_left = clamp_float(fmodf(data->n2_left, 12.0f), 0.0f, 15.0f);
    data->vib_right = clamp_float(fmodf(data->n2_right, 12.0f), 0.0f, 15.0f);
    data->oil_pressure_left = clamp_float(oil_pressure[0], 0.0f, 120.0f);
    data->oil_pressure_right = clamp_float(oil_pressure[1], 0.0f, 120.0f);
    data->oil_temp_left = clamp_float(oil_temp[0], 0.0f, 200.0f);
    data->oil_temp_right = clamp_float(oil_temp[1], 0.0f, 200.0f);
    data->oil_qty_left = clamp_float(oil_qty[0] * 20.0f, 0.0f, 40.0f);
    data->oil_qty_right = clamp_float(oil_qty[1] * 20.0f, 0.0f, 40.0f);
    data->fuel_flow_left = clamp_float(fuel_flow[0] * 3600.0f, 0.0f, 999.0f);
    data->fuel_flow_right = clamp_float(fuel_flow[1] * 3600.0f, 0.0f, 999.0f);
    data->source = EICAS2_SOURCE_XPLANE;
    data->xplane_connected = 1;
    return 0;
}

DWORD WINAPI eicas2_data_thread_proc(LPVOID param) {
    EICAS2ThreadContext *ctx = (EICAS2ThreadContext *)param;
    EICAS2Data local_data;
    FILE *file = NULL;
    XPCSocket sock;
    int file_backoff = 0;

    init_eicas2_defaults(&local_data);
    sock.sock = INVALID_SOCKET;
    InterlockedExchange(&g_eicas2_thread_running, 1);
    if (ctx != NULL && ctx->mode != EICAS2_MODE_XPLANE) file = open_eicas2_data_file(ctx->file_path);
    if (ctx != NULL && ctx->mode != EICAS2_MODE_FILE) sock = openUDP(ctx->xp_port);

    while (InterlockedCompareExchange(&g_eicas2_exit_requested, 0, 0) == 0) {
        int has_update = 0;

        if (ctx != NULL && ctx->mode != EICAS2_MODE_FILE && sock.sock != INVALID_SOCKET) {
            if (getEICAS2Data(sock, &local_data) == 0) {
                has_update = 1;
                Sleep(500);
            } else if (ctx->mode == EICAS2_MODE_XPLANE) {
                local_data.source = EICAS2_SOURCE_STATIC;
                local_data.xplane_connected = 0;
                Sleep(250);
                has_update = 1;
            }
        }

        if (!has_update && ctx != NULL && ctx->mode != EICAS2_MODE_XPLANE) {
            if (file == NULL && file_backoff == 0) {
                file = open_eicas2_data_file(ctx->file_path);
                if (file == NULL) file_backoff = 30;
            }

            if (file != NULL && load_next_eicas2_frame(file, &local_data)) {
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
            local_data.source = EICAS2_SOURCE_STATIC;
            local_data.xplane_connected = 0;
            Sleep(100);
            has_update = 1;
        }

        if (has_update && InterlockedCompareExchange(&g_eicas2_data_ready, 0, 0) == 0) {
            g_shared_eicas2_data = local_data;
            InterlockedExchange(&g_eicas2_data_ready, 1);
        }
    }

    if (file != NULL) fclose(file);
    if (sock.sock != INVALID_SOCKET) closeUDP(sock);
    InterlockedExchange(&g_eicas2_thread_running, 0);
    return 0;
}

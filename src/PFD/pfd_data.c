#include "pfd_data.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define METERS_TO_FEET 3.28084f

volatile LONG g_pfd_data_ready = 0;
volatile LONG g_pfd_exit_requested = 0;
volatile LONG g_pfd_thread_running = 0;
PFDData g_shared_pfd_data;

int current_airspeed = 157;
int target_airspeed = 120;
int current_altitude = 2128;
int target_altitude = 1100;
int agl_altitude = 1628;
float current_pitch = 2.0f;
float current_roll = 7.0f;
int current_vertical_speed = 321;
int current_heading = 182;
int target_heading = 20;
int current_thrust = 10;
int current_xplane_connected = 0;
PFDDataSource current_data_source = PFD_SOURCE_STATIC;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int normalize_heading(int heading) {
    int value = heading % 360;
    if (value < 0) {
        value += 360;
    }
    return value;
}

static int round_to_int(float value) {
    return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

void init_pfd_defaults(PFDData *data) {
    if (data == NULL) {
        return;
    }

    data->current_airspeed = 157;
    data->target_airspeed = 120;
    data->current_altitude = 2128;
    data->target_altitude = 1100;
    data->agl_altitude = 1628;
    data->current_pitch = 2.0f;
    data->current_roll = 7.0f;
    data->current_vertical_speed = 321;
    data->current_heading = 182;
    data->target_heading = 20;
    data->current_thrust = 10;
    data->source = PFD_SOURCE_STATIC;
    data->xplane_connected = 0;
}

void sync_globals_from_pfd_data(const PFDData *data) {
    if (data == NULL) {
        return;
    }

    current_airspeed = data->current_airspeed;
    target_airspeed = data->target_airspeed;
    current_altitude = data->current_altitude;
    target_altitude = data->target_altitude;
    agl_altitude = data->agl_altitude;
    current_pitch = data->current_pitch;
    current_roll = data->current_roll;
    current_vertical_speed = data->current_vertical_speed;
    current_heading = data->current_heading;
    target_heading = data->target_heading;
    current_thrust = data->current_thrust;
    current_xplane_connected = data->xplane_connected;
    current_data_source = data->source;
}

FILE *open_pfd_data_file(const char *path) {
    const char *final_path = path == NULL ? PFD_DATA_FILE_PATH : path;
    return fopen(final_path, "r");
}

int read_pfd_data(FILE *file, PFDData *data) {
    char line[256];
    int result = 0;
    int cur_airspeed = 0;
    int tgt_airspeed = 0;
    int cur_altitude = 0;
    int tgt_altitude = 0;
    int agl = 0;
    int pitch = 0;
    int roll = 0;
    int vsi = 0;
    int cur_heading = 0;
    int tgt_heading = 0;
    int thrust = 0;

    if (file == NULL || data == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        return 0;
    }

    result = sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &cur_airspeed, &tgt_airspeed, &cur_altitude, &tgt_altitude, &agl,
        &pitch, &roll, &vsi, &cur_heading, &tgt_heading, &thrust);
    if (result != 11) {
        return 0;
    }

    data->current_airspeed = clamp_int(cur_airspeed, AIRSPEED_MIN, AIRSPEED_MAX);
    data->target_airspeed = clamp_int(tgt_airspeed, AIRSPEED_MIN, AIRSPEED_MAX);
    data->current_altitude = clamp_int(cur_altitude, ALTITUDE_MIN, ALTITUDE_MAX);
    data->target_altitude = clamp_int(tgt_altitude, ALTITUDE_MIN, ALTITUDE_MAX);
    data->agl_altitude = clamp_int(agl, ALTITUDE_MIN, ALTITUDE_MAX);
    data->current_pitch = (float)pitch;
    data->current_roll = (float)roll;
    data->current_vertical_speed = clamp_int(vsi, VERTICAL_SPEED_MIN, VERTICAL_SPEED_MAX);
    data->current_heading = normalize_heading(cur_heading);
    data->target_heading = normalize_heading(tgt_heading);
    data->current_thrust = clamp_int(thrust, THRUST_MIN, THRUST_MAX);
    data->source = PFD_SOURCE_FILE;
    data->xplane_connected = 0;
    return 1;
}

int load_next_file_frame(FILE *file, PFDData *data) {
    if (read_pfd_data(file, data)) {
        return 1;
    }

    if (file == NULL) {
        return 0;
    }

    fseek(file, 0, SEEK_SET);
    return read_pfd_data(file, data);
}

PFDDataMode get_pfd_data_mode_from_env(void) {
    const char *mode = getenv("PFD_DATA_MODE");

    if (mode == NULL || *mode == '\0') {
        return PFD_MODE_AUTO;
    }

    if (_stricmp(mode, "file") == 0) {
        return PFD_MODE_FILE;
    }
    if (_stricmp(mode, "xplane") == 0) {
        return PFD_MODE_XPLANE;
    }
    return PFD_MODE_AUTO;
}

unsigned short get_xplane_port_from_env(void) {
    const char *port_text = getenv("PFD_XPLANE_PORT");
    long port = 0;

    if (port_text == NULL || *port_text == '\0') {
        return XPC_DEFAULT_PORT;
    }

    port = strtol(port_text, NULL, 10);
    if (port <= 0 || port > 65535) {
        return XPC_DEFAULT_PORT;
    }
    return (unsigned short)port;
}

int getPFDData(XPCSocket sock, PFDData *data) {
    // 尽量把一次界面需要的字段放进同一批 DREF 请求，避免多次 UDP 往返导致帧内数据不一致。
    const char *drefs[] = {
        "sim/cockpit2/gauges/indicators/airspeed_kts_pilot",
        "sim/cockpit/autopilot/airspeed",
        "sim/flightmodel/position/elevation",
        "sim/cockpit/autopilot/altitude",
        "sim/flightmodel/position/y_agl",
        "sim/flightmodel/position/theta",
        "sim/flightmodel/position/phi",
        "sim/cockpit2/gauges/indicators/vvi_fpm_pilot",
        "sim/cockpit2/gauges/indicators/heading_AHARS_deg_mag_pilot",
        "sim/cockpit/autopilot/heading_mag",
        "sim/cockpit2/engine/actuators/throttle_ratio"
    };
    float airspeed = 0.0f;
    float target_spd = 0.0f;
    float altitude_m = 0.0f;
    float target_alt_ft = 0.0f;
    float agl_m = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float vertical_speed = 0.0f;
    float heading = 0.0f;
    float target_hdg = 0.0f;
    float throttle_ratio[8] = {0.0f};
    float *values[] = {
        &airspeed,
        &target_spd,
        &altitude_m,
        &target_alt_ft,
        &agl_m,
        &pitch,
        &roll,
        &vertical_speed,
        &heading,
        &target_hdg,
        throttle_ratio
    };
    int sizes[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8};
    int i = 0;
    float throttle_sum = 0.0f;

    if (data == NULL) {
        return -1;
    }

    if (getDREFs(sock, drefs, values, (unsigned char)(sizeof(values) / sizeof(values[0])), sizes) != 0) {
        data->xplane_connected = 0;
        return -2;
    }

    // X-Plane 油门返回 8 发动机数组，这里做平均后再映射到单个百分比指示器。
    for (i = 0; i < sizes[10]; ++i) {
        throttle_sum += throttle_ratio[i];
    }
    if (sizes[10] > 0) {
        throttle_sum /= (float)sizes[10];
    }

    data->current_airspeed = clamp_int(round_to_int(airspeed), AIRSPEED_MIN, AIRSPEED_MAX);
    data->target_airspeed = clamp_int(round_to_int(target_spd), AIRSPEED_MIN, AIRSPEED_MAX);
    data->current_altitude = clamp_int(round_to_int(altitude_m * METERS_TO_FEET), ALTITUDE_MIN, ALTITUDE_MAX);
    data->target_altitude = clamp_int(round_to_int(target_alt_ft), ALTITUDE_MIN, ALTITUDE_MAX);
    data->agl_altitude = clamp_int(round_to_int(agl_m * METERS_TO_FEET), ALTITUDE_MIN, ALTITUDE_MAX);
    data->current_pitch = pitch;
    data->current_roll = roll;
    data->current_vertical_speed = clamp_int(round_to_int(vertical_speed), VERTICAL_SPEED_MIN, VERTICAL_SPEED_MAX);
    data->current_heading = normalize_heading(round_to_int(heading));
    data->target_heading = normalize_heading(round_to_int(target_hdg));
    data->current_thrust = clamp_int(round_to_int(throttle_sum * 100.0f), THRUST_MIN, THRUST_MAX);
    data->source = PFD_SOURCE_XPLANE;
    data->xplane_connected = 1;

    printf("X-Plane PFD data: IAS=%d ALT=%d VS=%d HDG=%d THR=%d%%\n",
        data->current_airspeed,
        data->current_altitude,
        data->current_vertical_speed,
        data->current_heading,
        data->current_thrust);
    return 0;
}

DWORD WINAPI pfd_data_thread_proc(LPVOID param) {
    PFDThreadContext *ctx = (PFDThreadContext *)param;
    PFDData local_data;
    FILE *file = NULL;
    XPCSocket sock;
    int file_backoff = 0;

    init_pfd_defaults(&local_data);
    sock.sock = INVALID_SOCKET;
    InterlockedExchange(&g_pfd_thread_running, 1);

    if (ctx != NULL && ctx->mode != PFD_MODE_XPLANE) {
        file = open_pfd_data_file(ctx->file_path);
    }
    if (ctx != NULL && ctx->mode != PFD_MODE_FILE) {
        sock = openUDP(ctx->xp_port);
    }

    while (InterlockedCompareExchange(&g_pfd_exit_requested, 0, 0) == 0) {
        int has_update = 0;

        if (ctx != NULL && ctx->mode != PFD_MODE_FILE && sock.sock != INVALID_SOCKET) {
            if (getPFDData(sock, &local_data) == 0) {
                has_update = 1;
                // 实时链路优先稳定性，按任务要求主动降频，避免请求过密把 CPU 和模拟器都打满。
                Sleep(1000);
            } else if (ctx->mode == PFD_MODE_XPLANE) {
                local_data.source = PFD_SOURCE_STATIC;
                local_data.xplane_connected = 0;
                Sleep(250);
                has_update = 1;
            }
        }

        if (!has_update && ctx != NULL && ctx->mode != PFD_MODE_XPLANE) {
            if (file == NULL && file_backoff == 0) {
                file = open_pfd_data_file(ctx->file_path);
                if (file == NULL) {
                    // 文件不存在时做短暂退避，避免主循环期间持续空转重试。
                    file_backoff = 30;
                }
            }

            if (file != NULL && load_next_file_frame(file, &local_data)) {
                local_data.source = PFD_SOURCE_FILE;
                local_data.xplane_connected = 0;
                has_update = 1;
                Sleep(16);
            } else {
                if (file != NULL) {
                    fclose(file);
                    file = NULL;
                }
                if (file_backoff > 0) {
                    --file_backoff;
                }
                Sleep(100);
            }
        }

        if (!has_update) {
            local_data.source = PFD_SOURCE_STATIC;
            local_data.xplane_connected = 0;
            Sleep(100);
            has_update = 1;
        }

        if (has_update && InterlockedCompareExchange(&g_pfd_data_ready, 0, 0) == 0) {
            // 仅在主线程消费完上一帧后才覆盖共享数据，用 0/1 标志位保证一次只交付一份完整快照。
            g_shared_pfd_data = local_data;
            InterlockedExchange(&g_pfd_data_ready, 1);
        }
    }

    if (file != NULL) {
        fclose(file);
    }
    if (sock.sock != INVALID_SOCKET) {
        closeUDP(sock);
    }

    InterlockedExchange(&g_pfd_thread_running, 0);
    return 0;
}

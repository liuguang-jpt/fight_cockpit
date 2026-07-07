#include "map_data.h"

#include "../Util/xplaneConnect.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAP_FALLBACK_IMAGE_PATH "assets/assets/assets/20260303110928.png"
#define MAP_FALLBACK_CITY "Beijing"
#define MAP_FALLBACK_DISTRICT "Chaoyang"
#define MAP_FALLBACK_ADCODE "110105"
#define MAP_IMAGE_WIDTH 1024
#define MAP_IMAGE_HEIGHT 768
#define MAP_INITIAL_ZOOM 9

typedef struct MapBuffer {
    unsigned char *data;
    size_t size;
} MapBuffer;

static SDL_Thread *g_map_thread = NULL;
static SDL_mutex *g_map_mutex = NULL;
static int g_map_running = 0;
static int g_requested_zoom = MAP_INITIAL_ZOOM;
static MapSnapshot g_snapshot;
static unsigned char *g_image_bytes = NULL;
static size_t g_image_size = 0;
static unsigned int g_image_version = 0;

static void safe_copy(char *dst, size_t size, const char *src) {
    if (dst == NULL || size == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, size, "%s", src);
}

static void set_status(MapSnapshot *snapshot, const char *message) {
    if (snapshot == NULL) return;
    safe_copy(snapshot->status_line, sizeof(snapshot->status_line), message);
}

static void free_buffer(MapBuffer *buffer) {
    if (buffer == NULL) return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

static int load_file_buffer(const char *path, MapBuffer *buffer) {
    FILE *file = NULL;
    long file_size = 0;

    if (path == NULL || buffer == NULL) return 0;
    file = fopen(path, "rb");
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    buffer->data = (unsigned char *)malloc((size_t)file_size);
    if (buffer->data == NULL) {
        fclose(file);
        return 0;
    }
    if (fread(buffer->data, 1, (size_t)file_size, file) != (size_t)file_size) {
        fclose(file);
        free_buffer(buffer);
        return 0;
    }
    fclose(file);
    buffer->size = (size_t)file_size;
    return 1;
}

static size_t map_curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    MapBuffer *buffer = (MapBuffer *)userp;
    size_t chunk_size = size * nmemb;
    unsigned char *next = NULL;

    if (buffer == NULL || chunk_size == 0) return 0;
    next = (unsigned char *)realloc(buffer->data, buffer->size + chunk_size + 1);
    if (next == NULL) return 0;
    buffer->data = next;
    memcpy(buffer->data + buffer->size, contents, chunk_size);
    buffer->size += chunk_size;
    buffer->data[buffer->size] = '\0';
    return chunk_size;
}

static int http_get(const char *url, MapBuffer *buffer) {
    CURL *curl = NULL;
    CURLcode result;

    if (url == NULL || buffer == NULL) return 0;
    curl = curl_easy_init();
    if (curl == NULL) return 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2500L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "fight-cockpit-map/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, map_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && buffer->size > 0;
}

static cJSON *parse_json_buffer(const MapBuffer *buffer) {
    if (buffer == NULL || buffer->data == NULL || buffer->size == 0) return NULL;
    return cJSON_ParseWithLength((const char *)buffer->data, buffer->size);
}

static const char *json_string_or_null(const cJSON *item) {
    return cJSON_IsString(item) && item->valuestring != NULL ? item->valuestring : NULL;
}

static const char *json_city_value(const cJSON *item, const char *fallback) {
    if (cJSON_IsString(item) && item->valuestring != NULL && item->valuestring[0] != '\0') return item->valuestring;
    if (cJSON_IsArray(item) && cJSON_GetArraySize(item) > 0) {
        const cJSON *first = cJSON_GetArrayItem(item, 0);
        if (cJSON_IsString(first) && first->valuestring != NULL && first->valuestring[0] != '\0') return first->valuestring;
    }
    return fallback;
}

static void store_image_bytes(const MapBuffer *buffer) {
    unsigned char *next = NULL;

    if (buffer == NULL || buffer->data == NULL || buffer->size == 0) return;
    next = (unsigned char *)malloc(buffer->size);
    if (next == NULL) return;
    memcpy(next, buffer->data, buffer->size);

    SDL_LockMutex(g_map_mutex);
    free(g_image_bytes);
    g_image_bytes = next;
    g_image_size = buffer->size;
    ++g_image_version;
    SDL_UnlockMutex(g_map_mutex);
}

/* Track points are kept as a short rolling time series so the UI can render a recent flight trail. */
static void append_track_point(MapSnapshot *snapshot, double latitude, double longitude, float heading_deg, Uint32 tick_ms) {
    if (snapshot == NULL) return;
    if (snapshot->track_count < MAP_TRACK_CAPACITY) {
        snapshot->track[snapshot->track_count].latitude = latitude;
        snapshot->track[snapshot->track_count].longitude = longitude;
        snapshot->track[snapshot->track_count].heading_deg = heading_deg;
        snapshot->track[snapshot->track_count].tick_ms = tick_ms;
        ++snapshot->track_count;
        return;
    }

    memmove(snapshot->track, snapshot->track + 1, sizeof(MapTrackPoint) * (MAP_TRACK_CAPACITY - 1));
    snapshot->track[MAP_TRACK_CAPACITY - 1].latitude = latitude;
    snapshot->track[MAP_TRACK_CAPACITY - 1].longitude = longitude;
    snapshot->track[MAP_TRACK_CAPACITY - 1].heading_deg = heading_deg;
    snapshot->track[MAP_TRACK_CAPACITY - 1].tick_ms = tick_ms;
}

float map_calculate_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double delta_lambda = (lon2 - lon1) * M_PI / 180.0;
    double y = sin(delta_lambda) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(delta_lambda);
    double bearing = atan2(y, x) * 180.0 / M_PI;

    if (bearing < 0.0) bearing += 360.0;
    return (float)bearing;
}

static void set_fallback_city_weather(MapSnapshot *snapshot) {
    if (snapshot == NULL) return;
    safe_copy(snapshot->city, sizeof(snapshot->city), MAP_FALLBACK_CITY);
    safe_copy(snapshot->district, sizeof(snapshot->district), MAP_FALLBACK_DISTRICT);
    safe_copy(snapshot->adcode, sizeof(snapshot->adcode), MAP_FALLBACK_ADCODE);
    safe_copy(snapshot->address, sizeof(snapshot->address), "Beijing Capital Airport Corridor");
    safe_copy(snapshot->weather_text, sizeof(snapshot->weather_text), "CLEAR");
    safe_copy(snapshot->temperature_c, sizeof(snapshot->temperature_c), "26");
    safe_copy(snapshot->humidity, sizeof(snapshot->humidity), "34");
    safe_copy(snapshot->wind_direction, sizeof(snapshot->wind_direction), "NORTHWEST");
    safe_copy(snapshot->wind_power, sizeof(snapshot->wind_power), "3");
    safe_copy(snapshot->report_time, sizeof(snapshot->report_time), "LOCAL FALLBACK");
    safe_copy(snapshot->weather_source, sizeof(snapshot->weather_source), "LOCAL");
    safe_copy(snapshot->map_source, sizeof(snapshot->map_source), "LOCAL");
}

static int fetch_static_map_png(const char *api_key, double latitude, double longitude, int zoom_level, MapBuffer *buffer) {
    char url[512];

    if (api_key == NULL || *api_key == '\0') return 0;
    snprintf(
        url,
        sizeof(url),
        "https://restapi.amap.com/v3/staticmap?location=%.6f,%.6f&zoom=%d&size=%d*%d&scale=2&traffic=1&markers=mid,0x0091ff,A:%.6f,%.6f&key=%s",
        longitude,
        latitude,
        zoom_level,
        MAP_IMAGE_WIDTH / 2,
        MAP_IMAGE_HEIGHT / 2,
        longitude,
        latitude,
        api_key
    );
    return http_get(url, buffer);
}

/* Reverse geocode gives us city and district labels without hard-coding a region. */
static int fetch_reverse_geocode(const char *api_key, double latitude, double longitude, MapSnapshot *snapshot) {
    MapBuffer buffer = {0};
    cJSON *root = NULL;
    cJSON *regeocode = NULL;
    cJSON *component = NULL;
    const char *status = NULL;
    char url[512];
    int ok = 0;

    if (api_key == NULL || *api_key == '\0' || snapshot == NULL) return 0;
    snprintf(
        url,
        sizeof(url),
        "https://restapi.amap.com/v3/geocode/regeo?location=%.6f,%.6f&extensions=base&radius=1000&key=%s",
        longitude,
        latitude,
        api_key
    );
    if (!http_get(url, &buffer)) return 0;

    root = parse_json_buffer(&buffer);
    if (root == NULL) goto cleanup;
    status = json_string_or_null(cJSON_GetObjectItemCaseSensitive(root, "status"));
    if (status == NULL || strcmp(status, "1") != 0) goto cleanup;

    regeocode = cJSON_GetObjectItemCaseSensitive(root, "regeocode");
    component = regeocode == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(regeocode, "addressComponent");
    if (regeocode == NULL || component == NULL) goto cleanup;

    safe_copy(snapshot->address, sizeof(snapshot->address), json_string_or_null(cJSON_GetObjectItemCaseSensitive(regeocode, "formatted_address")));
    safe_copy(snapshot->district, sizeof(snapshot->district), json_string_or_null(cJSON_GetObjectItemCaseSensitive(component, "district")));
    safe_copy(snapshot->adcode, sizeof(snapshot->adcode), json_string_or_null(cJSON_GetObjectItemCaseSensitive(component, "adcode")));
    safe_copy(
        snapshot->city,
        sizeof(snapshot->city),
        json_city_value(
            cJSON_GetObjectItemCaseSensitive(component, "city"),
            json_string_or_null(cJSON_GetObjectItemCaseSensitive(component, "province"))
        )
    );
    ok = snapshot->city[0] != '\0';

cleanup:
    if (root != NULL) cJSON_Delete(root);
    free_buffer(&buffer);
    return ok;
}

static int fetch_weather_info(const char *api_key, const char *adcode, MapSnapshot *snapshot) {
    MapBuffer buffer = {0};
    cJSON *root = NULL;
    cJSON *lives = NULL;
    cJSON *first = NULL;
    const char *status = NULL;
    char url[512];
    int ok = 0;

    if (api_key == NULL || *api_key == '\0' || adcode == NULL || *adcode == '\0' || snapshot == NULL) return 0;
    snprintf(
        url,
        sizeof(url),
        "https://restapi.amap.com/v3/weather/weatherInfo?city=%s&extensions=base&key=%s",
        adcode,
        api_key
    );
    if (!http_get(url, &buffer)) return 0;

    root = parse_json_buffer(&buffer);
    if (root == NULL) goto cleanup;
    status = json_string_or_null(cJSON_GetObjectItemCaseSensitive(root, "status"));
    if (status == NULL || strcmp(status, "1") != 0) goto cleanup;

    lives = cJSON_GetObjectItemCaseSensitive(root, "lives");
    if (!cJSON_IsArray(lives) || cJSON_GetArraySize(lives) == 0) goto cleanup;
    first = cJSON_GetArrayItem(lives, 0);
    if (first == NULL) goto cleanup;

    safe_copy(snapshot->city, sizeof(snapshot->city), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "city")));
    safe_copy(snapshot->weather_text, sizeof(snapshot->weather_text), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "weather")));
    safe_copy(snapshot->temperature_c, sizeof(snapshot->temperature_c), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "temperature")));
    safe_copy(snapshot->wind_direction, sizeof(snapshot->wind_direction), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "winddirection")));
    safe_copy(snapshot->wind_power, sizeof(snapshot->wind_power), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "windpower")));
    safe_copy(snapshot->humidity, sizeof(snapshot->humidity), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "humidity")));
    safe_copy(snapshot->report_time, sizeof(snapshot->report_time), json_string_or_null(cJSON_GetObjectItemCaseSensitive(first, "reporttime")));
    safe_copy(snapshot->weather_source, sizeof(snapshot->weather_source), "AMAP");
    ok = snapshot->weather_text[0] != '\0';

cleanup:
    if (root != NULL) cJSON_Delete(root);
    free_buffer(&buffer);
    return ok;
}

static int fetch_xplane_position(XPCSocket socket_handle, double *latitude, double *longitude, float *heading_deg, float *groundspeed_kts) {
    const char *drefs[] = {
        "sim/flightmodel/position/latitude",
        "sim/flightmodel/position/longitude",
        "sim/flightmodel/position/psi",
        "sim/flightmodel/position/groundspeed"
    };
    float lat_value[1] = {0.0f};
    float lon_value[1] = {0.0f};
    float psi_value[1] = {0.0f};
    float speed_value[1] = {0.0f};
    float *values[] = {lat_value, lon_value, psi_value, speed_value};
    int sizes[] = {1, 1, 1, 1};

    if (socket_handle.sock == INVALID_SOCKET) return 0;
    if (getDREFs(socket_handle, drefs, values, 4, sizes) != 0) return 0;

    *latitude = lat_value[0];
    *longitude = lon_value[0];
    *heading_deg = psi_value[0];
    *groundspeed_kts = speed_value[0] * 1.943844f;
    return 1;
}

static void simulate_position(MapSnapshot *snapshot, Uint32 elapsed_ms) {
    double t = (double)elapsed_ms / 1000.0;
    double latitude = 40.060000 + 0.085 * sin(t * 0.12);
    double longitude = 116.510000 + 0.125 * cos(t * 0.12);
    float heading = snapshot->track_count == 0
        ? 82.0f
        : map_calculate_bearing_deg(
            snapshot->aircraft_latitude,
            snapshot->aircraft_longitude,
            latitude,
            longitude
        );

    snapshot->aircraft_latitude = latitude;
    snapshot->aircraft_longitude = longitude;
    snapshot->center_latitude = latitude;
    snapshot->center_longitude = longitude;
    snapshot->aircraft_heading_deg = heading;
    snapshot->groundspeed_kts = 225.0f + (float)(18.0 * sin(t * 0.18));
    safe_copy(snapshot->position_source, sizeof(snapshot->position_source), "SIM");
}

static void publish_snapshot(const MapSnapshot *snapshot) {
    SDL_LockMutex(g_map_mutex);
    g_snapshot = *snapshot;
    SDL_UnlockMutex(g_map_mutex);
}

static void load_fallback_image_once(void) {
    MapBuffer buffer = {0};

    if (!load_file_buffer(MAP_FALLBACK_IMAGE_PATH, &buffer)) return;
    store_image_bytes(&buffer);
    free_buffer(&buffer);
}

static int map_thread_main(void *unused) {
    const char *api_key = getenv("AMAP_API_KEY");
    XPCSocket xp_socket = openUDP(0);
    MapSnapshot local = g_snapshot;
    Uint32 last_remote_refresh = 0;
    double last_refresh_lat = 0.0;
    double last_refresh_lon = 0.0;
    int last_refresh_zoom = local.zoom_level;

    (void)unused;
    while (g_map_running) {
        Uint32 now = SDL_GetTicks();
        int requested_zoom = MAP_INITIAL_ZOOM;
        int need_remote_refresh = 0;
        double lat = 0.0;
        double lon = 0.0;
        float heading = 0.0f;
        float groundspeed = 0.0f;

        SDL_LockMutex(g_map_mutex);
        requested_zoom = g_requested_zoom;
        SDL_UnlockMutex(g_map_mutex);
        if (requested_zoom < 3) requested_zoom = 3;
        if (requested_zoom > 16) requested_zoom = 16;
        local.zoom_level = requested_zoom;

        if (fetch_xplane_position(xp_socket, &lat, &lon, &heading, &groundspeed)) {
            local.aircraft_latitude = lat;
            local.aircraft_longitude = lon;
            local.center_latitude = lat;
            local.center_longitude = lon;
            local.aircraft_heading_deg = heading;
            local.groundspeed_kts = groundspeed;
            safe_copy(local.position_source, sizeof(local.position_source), "XPLANE");
        } else {
            simulate_position(&local, now);
        }

        append_track_point(&local, local.aircraft_latitude, local.aircraft_longitude, local.aircraft_heading_deg, now);

        if (api_key != NULL && *api_key != '\0') {
            double distance_lat = fabs(local.aircraft_latitude - last_refresh_lat);
            double distance_lon = fabs(local.aircraft_longitude - last_refresh_lon);

            need_remote_refresh = last_remote_refresh == 0
                || requested_zoom != last_refresh_zoom
                || now - last_remote_refresh > 12000
                || distance_lat > 0.020
                || distance_lon > 0.020;
        }

        if (need_remote_refresh) {
            MapBuffer image_buffer = {0};
            int map_ok = fetch_static_map_png(api_key, local.center_latitude, local.center_longitude, requested_zoom, &image_buffer);
            int geocode_ok = fetch_reverse_geocode(api_key, local.center_latitude, local.center_longitude, &local);
            int weather_ok = geocode_ok && fetch_weather_info(api_key, local.adcode, &local);

            if (map_ok) {
                store_image_bytes(&image_buffer);
                safe_copy(local.map_source, sizeof(local.map_source), "AMAP");
            } else {
                safe_copy(local.map_source, sizeof(local.map_source), "LOCAL");
            }
            if (!geocode_ok || !weather_ok) {
                set_fallback_city_weather(&local);
                set_status(&local, "AMAP partial failure - using local city/weather fallback");
            } else {
                set_status(&local, "AMAP map, reverse geocode and weather online");
            }

            free_buffer(&image_buffer);
            last_remote_refresh = now;
            last_refresh_lat = local.aircraft_latitude;
            last_refresh_lon = local.aircraft_longitude;
            last_refresh_zoom = requested_zoom;
        } else if (api_key == NULL || *api_key == '\0') {
            set_fallback_city_weather(&local);
            set_status(&local, "Set AMAP_API_KEY to enable live map and weather");
        }

        publish_snapshot(&local);
        SDL_Delay(500);
    }

    closeUDP(xp_socket);
    return 0;
}

int map_init_data(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_map_mutex = SDL_CreateMutex();
    if (g_map_mutex == NULL) return -1;

    memset(&g_snapshot, 0, sizeof(g_snapshot));
    g_snapshot.center_latitude = 40.060000;
    g_snapshot.center_longitude = 116.510000;
    g_snapshot.aircraft_latitude = 40.060000;
    g_snapshot.aircraft_longitude = 116.510000;
    g_snapshot.aircraft_heading_deg = 82.0f;
    g_snapshot.groundspeed_kts = 220.0f;
    g_snapshot.zoom_level = MAP_INITIAL_ZOOM;
    safe_copy(g_snapshot.position_source, sizeof(g_snapshot.position_source), "SIM");
    set_fallback_city_weather(&g_snapshot);
    set_status(&g_snapshot, "MAP ready with local fallback data");

    load_fallback_image_once();
    g_map_running = 1;
    g_map_thread = SDL_CreateThread(map_thread_main, "map-data", NULL);
    if (g_map_thread == NULL) {
        g_map_running = 0;
        return -1;
    }
    return 0;
}

void map_shutdown_data(void) {
    g_map_running = 0;
    if (g_map_thread != NULL) {
        SDL_WaitThread(g_map_thread, NULL);
        g_map_thread = NULL;
    }
    if (g_map_mutex != NULL) {
        SDL_DestroyMutex(g_map_mutex);
        g_map_mutex = NULL;
    }
    free(g_image_bytes);
    g_image_bytes = NULL;
    g_image_size = 0;
    curl_global_cleanup();
}

void map_get_snapshot(MapSnapshot *out_snapshot) {
    if (out_snapshot == NULL || g_map_mutex == NULL) return;
    SDL_LockMutex(g_map_mutex);
    *out_snapshot = g_snapshot;
    SDL_UnlockMutex(g_map_mutex);
}

int map_copy_latest_image(unsigned char **out_bytes, size_t *out_size, unsigned int *out_version) {
    unsigned char *copy = NULL;

    if (out_bytes == NULL || out_size == NULL || out_version == NULL || g_map_mutex == NULL) return 0;
    *out_bytes = NULL;
    *out_size = 0;
    *out_version = 0;

    SDL_LockMutex(g_map_mutex);
    if (g_image_bytes == NULL || g_image_size == 0) {
        SDL_UnlockMutex(g_map_mutex);
        return 0;
    }
    copy = (unsigned char *)malloc(g_image_size);
    if (copy != NULL) memcpy(copy, g_image_bytes, g_image_size);
    *out_size = g_image_size;
    *out_version = g_image_version;
    SDL_UnlockMutex(g_map_mutex);

    if (copy == NULL) return 0;
    *out_bytes = copy;
    return 1;
}

void map_request_zoom_delta(int delta) {
    if (g_map_mutex == NULL) return;
    SDL_LockMutex(g_map_mutex);
    g_requested_zoom += delta;
    if (g_requested_zoom < 3) g_requested_zoom = 3;
    if (g_requested_zoom > 16) g_requested_zoom = 16;
    SDL_UnlockMutex(g_map_mutex);
}

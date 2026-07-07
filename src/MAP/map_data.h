#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <stddef.h>

#include <SDL2/SDL.h>

#define MAP_TRACK_CAPACITY 160
#define MAP_CITY_LEN 64
#define MAP_DISTRICT_LEN 64
#define MAP_ADCODE_LEN 16
#define MAP_ADDRESS_LEN 128
#define MAP_WEATHER_LEN 32
#define MAP_STATUS_LEN 128
#define MAP_SOURCE_LEN 16

typedef struct MapTrackPoint {
    double latitude;
    double longitude;
    float heading_deg;
    Uint32 tick_ms;
} MapTrackPoint;

typedef struct MapSnapshot {
    double center_latitude;
    double center_longitude;
    double aircraft_latitude;
    double aircraft_longitude;
    float aircraft_heading_deg;
    float groundspeed_kts;
    int zoom_level;
    char city[MAP_CITY_LEN];
    char district[MAP_DISTRICT_LEN];
    char adcode[MAP_ADCODE_LEN];
    char address[MAP_ADDRESS_LEN];
    char weather_text[MAP_WEATHER_LEN];
    char temperature_c[16];
    char humidity[16];
    char wind_direction[32];
    char wind_power[16];
    char report_time[32];
    char status_line[MAP_STATUS_LEN];
    char map_source[MAP_SOURCE_LEN];
    char weather_source[MAP_SOURCE_LEN];
    char position_source[MAP_SOURCE_LEN];
    MapTrackPoint track[MAP_TRACK_CAPACITY];
    int track_count;
} MapSnapshot;

int map_init_data(void);
void map_shutdown_data(void);
void map_get_snapshot(MapSnapshot *out_snapshot);
int map_copy_latest_image(unsigned char **out_bytes, size_t *out_size, unsigned int *out_version);
void map_request_zoom_delta(int delta);
float map_calculate_bearing_deg(double lat1, double lon1, double lat2, double lon2);

#endif

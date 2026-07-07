#ifndef FMC_ND_LINK_H
#define FMC_ND_LINK_H

#include <stddef.h>
#include <windows.h>

#define FMC_ND_LINK_ROUTE_CAPACITY 64
#define FMC_ND_LINK_NAME_LEN 8

typedef struct FmcNdLinkRouteItem {
    char name[FMC_ND_LINK_NAME_LEN];
    double latitude;
    double longitude;
    char kind[8];
} FmcNdLinkRouteItem;

typedef struct FmcNdSharedRoute {
    DWORD signature;
    DWORD version;
    char origin[FMC_ND_LINK_NAME_LEN];
    char destination[FMC_ND_LINK_NAME_LEN];
    size_t route_count;
    ULONGLONG updated_tick_ms;
    FmcNdLinkRouteItem route[FMC_ND_LINK_ROUTE_CAPACITY];
} FmcNdSharedRoute;

int fmc_nd_link_publish(const char *origin, const char *destination, const FmcNdLinkRouteItem *route, size_t route_count);
int fmc_nd_link_read(FmcNdSharedRoute *out_route);
void fmc_nd_link_shutdown(void);

#endif

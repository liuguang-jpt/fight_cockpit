#include "fmc_nd_link.h"

#include <stdio.h>
#include <string.h>

#define FMC_ND_LINK_MAPPING_NAME "Local\\FightCockpitFmcNdRoute"
#define FMC_ND_LINK_SIGNATURE 0x464D434EUL

static HANDLE g_fmc_nd_mapping = NULL;
static FmcNdSharedRoute *g_fmc_nd_view = NULL;

static void safe_copy(char *dst, size_t size, const char *src) {
    if (dst == NULL || size == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, size, "%s", src);
}

static int ensure_mapping(void) {
    if (g_fmc_nd_view != NULL) return 1;

    g_fmc_nd_mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        (DWORD)sizeof(FmcNdSharedRoute),
        FMC_ND_LINK_MAPPING_NAME
    );
    if (g_fmc_nd_mapping == NULL) return 0;

    g_fmc_nd_view = (FmcNdSharedRoute *)MapViewOfFile(
        g_fmc_nd_mapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(FmcNdSharedRoute)
    );
    if (g_fmc_nd_view == NULL) {
        CloseHandle(g_fmc_nd_mapping);
        g_fmc_nd_mapping = NULL;
        return 0;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS || g_fmc_nd_view->signature != FMC_ND_LINK_SIGNATURE) {
        memset(g_fmc_nd_view, 0, sizeof(FmcNdSharedRoute));
        g_fmc_nd_view->signature = FMC_ND_LINK_SIGNATURE;
    }
    return 1;
}

int fmc_nd_link_publish(const char *origin, const char *destination, const FmcNdLinkRouteItem *route, size_t route_count) {
    size_t copy_count = route_count;

    if (!ensure_mapping()) return 0;
    if (copy_count > FMC_ND_LINK_ROUTE_CAPACITY) copy_count = FMC_ND_LINK_ROUTE_CAPACITY;

    memset(g_fmc_nd_view->origin, 0, sizeof(g_fmc_nd_view->origin));
    memset(g_fmc_nd_view->destination, 0, sizeof(g_fmc_nd_view->destination));
    safe_copy(g_fmc_nd_view->origin, sizeof(g_fmc_nd_view->origin), origin);
    safe_copy(g_fmc_nd_view->destination, sizeof(g_fmc_nd_view->destination), destination);
    memset(g_fmc_nd_view->route, 0, sizeof(g_fmc_nd_view->route));
    if (route != NULL && copy_count > 0) memcpy(g_fmc_nd_view->route, route, sizeof(FmcNdLinkRouteItem) * copy_count);
    g_fmc_nd_view->route_count = copy_count;
    g_fmc_nd_view->updated_tick_ms = GetTickCount64();
    ++g_fmc_nd_view->version;
    return 1;
}

int fmc_nd_link_read(FmcNdSharedRoute *out_route) {
    if (out_route == NULL) return 0;
    if (!ensure_mapping()) return 0;
    if (g_fmc_nd_view->signature != FMC_ND_LINK_SIGNATURE) return 0;
    *out_route = *g_fmc_nd_view;
    return 1;
}

void fmc_nd_link_shutdown(void) {
    if (g_fmc_nd_view != NULL) {
        UnmapViewOfFile(g_fmc_nd_view);
        g_fmc_nd_view = NULL;
    }
    if (g_fmc_nd_mapping != NULL) {
        CloseHandle(g_fmc_nd_mapping);
        g_fmc_nd_mapping = NULL;
    }
}

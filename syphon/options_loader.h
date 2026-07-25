#pragma once
#include <stdbool.h>

typedef struct {
    bool disablePAC;
    bool useLegacyAmmonia;
    bool pauseInjection;
    char **enabledTweaks;
    int enabledTweakCount;
} FangsOptions;

FangsOptions fangs_load_options(void);
char* fangs_build_dyld_insert_libraries(bool useLegacyAmmonia, const char* path);

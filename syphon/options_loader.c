#include "options_loader.h"
#include "tweak_utils.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FangsOptions fangs_load_options(void) {
    FangsOptions opts = {false, false, false, NULL, 0};
    const char* path = "/opt/pluginplayground/current.options";

    FILE* f = fopen(path, "rb");
    if (!f) return opts;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return opts; }
    fseek(f, 0, SEEK_SET);

    char* buf = malloc((size_t)len);
    if (!buf) { fclose(f); return opts; }
    if (!check_file_read(f, buf, (size_t)len)) {
        free(buf);
        fclose(f);
        return opts;
    }
    fclose(f);

    CFDataRef cfData = CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault, (const UInt8*)buf, (CFIndex)len, kCFAllocatorNull);
    if (!cfData) { free(buf); return opts; }

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, cfData, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(cfData);
    free(buf);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist) CFRelease(plist);
        return opts;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;

    CFBooleanRef val;
    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("disablePAC"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.disablePAC = (bool)CFBooleanGetValue(val);

    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("useLegacyAmmonia"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.useLegacyAmmonia = (bool)CFBooleanGetValue(val);

    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("pauseInjection"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.pauseInjection = (bool)CFBooleanGetValue(val);

    CFArrayRef enabledArr = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("enabledTweaks"));
    if (enabledArr && CFGetTypeID(enabledArr) == CFArrayGetTypeID()) {
        CFIndex count = CFArrayGetCount(enabledArr);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef s = (CFStringRef)CFArrayGetValueAtIndex(enabledArr, i);
            if (s && CFGetTypeID(s) == CFStringGetTypeID()) {
                char name[PATH_MAX];
                if (CFStringGetCString(s, name, sizeof(name), kCFStringEncodingUTF8)) {
                    char **tmp = realloc(opts.enabledTweaks,
                        (size_t)(opts.enabledTweakCount + 1) * sizeof(char *));
                    if (tmp) {
                        opts.enabledTweaks = tmp;
                        opts.enabledTweaks[opts.enabledTweakCount] = strdup(name);
                        if (opts.enabledTweaks[opts.enabledTweakCount])
                            opts.enabledTweakCount++;
                    }
                }
            }
        }
    }

    CFRelease(dict);
    return opts;
}

char* fangs_build_dyld_insert_libraries(bool useLegacyAmmonia, const char* path) {
    const char* dir = useLegacyAmmonia
        ? "/private/var/ammonia/core/tweaks"
        : "/opt/pluginplayground/tweaks";

    char* exe_path = get_exe_path();

    DIR* d = opendir(dir);
    if (!d) { free(exe_path); return NULL; }

    size_t total = 0;
    int count = 0;
    char** paths = NULL;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        const char* name = entry->d_name;

        size_t nlen = strlen(name);
        if (nlen <= 6 || strcmp(name + nlen - 6, ".dylib") != 0)
            continue;

        if (!should_load_tweak(dir, name, path))
            continue;

        if (!is_tweak_enabled(name))
            continue;

        if (!useLegacyAmmonia && !check_dylib_options(dir, name, path))
            continue;

        size_t plen = strlen(dir) + 1 + nlen + 1;
        char* full = malloc(plen);
        if (!full) continue;
        snprintf(full, plen, "%s/%s", dir, name);

        if (!is_tweak_safe(full)) {
            free(full);
            continue;
        }

        char** tmp = realloc(paths, sizeof(char*) * (size_t)(count + 1));
        if (!tmp) {
            free(full);
            continue;
        }
        paths = tmp;
        total += plen;
        count++;
        paths[count - 1] = full;
    }
    closedir(d);
    free(exe_path);

    if (count == 0) {
        free(paths);
        return NULL;
    }

    size_t needed = total + 1;
    char* result = malloc(needed);
    if (!result) {
        for (int i = 0; i < count; i++) free(paths[i]);
        free(paths);
        return NULL;
    }
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(result, ":");
        strcat(result, paths[i]);
        free(paths[i]);
    }
    free(paths);
    return result;
}

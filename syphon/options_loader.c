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
    CFDictionaryRef dict = fangs_read_plist_dictionary(
        "/opt/pluginplayground/current.options");
    if (!dict) return opts;

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

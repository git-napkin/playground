#include "options.h"
#include "file_utils.h"
#include <CoreFoundation/CoreFoundation.h>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *optionsPath() {
    return "/opt/pluginplayground/current.options";
}

Options loadOptions() {
    CFDataRef data = fileRead(optionsPath());
    if (!data) return {};

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
    CFRelease(data);
    if (!plist) return {};
    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return {};
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;
    Options opts;

    auto getBool = [&](CFStringRef key, bool fallback) {
        CFBooleanRef val = (CFBooleanRef)CFDictionaryGetValue(dict, key);
        if (!val || CFGetTypeID(val) != CFBooleanGetTypeID())
            return fallback;
        return (bool)CFBooleanGetValue(val);
    };

    opts.useLegacyAmmonia = getBool(CFSTR("useLegacyAmmonia"), false);
    opts.disablePAC = getBool(CFSTR("disablePAC"), false);
    opts.pauseInjection = getBool(CFSTR("pauseInjection"), false);

    CFRelease(dict);
    return opts;
}

static bool runPrivilegedScript(const char *script) {
    pid_t pid;
    const char *args[] = {"/usr/bin/osascript", "-e", script, nullptr};
    int r = posix_spawn(&pid, "/usr/bin/osascript", nullptr, nullptr,
                        (char *const *)args, nullptr);
    if (r != 0) return false;
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool fixPermissions() {
    return runPrivilegedScript(
        "do shell script \""
        "mkdir -p /opt/pluginplayground && "
        "touch /opt/pluginplayground/current.options && "
        "chmod 644 /opt/pluginplayground/current.options"
        "\" with administrator privileges");
}

bool saveOptions(const Options &opts) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR("useLegacyAmmonia"),
        opts.useLegacyAmmonia ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("disablePAC"),
        opts.disablePAC ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("pauseInjection"),
        opts.pauseInjection ? kCFBooleanTrue : kCFBooleanFalse);

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    CFRelease(dict);

    if (!data) return false;
    bool ok = fileWrite(optionsPath(), data);
    if (!ok) {
        if (fixPermissions())
            ok = fileWrite(optionsPath(), data);
    }
    CFRelease(data);
    return ok;
}

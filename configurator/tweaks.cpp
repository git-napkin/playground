#include "tweaks.h"
#include "file_utils.h"
#include "options.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>

std::string tweaksDir() {
    Options opts = loadOptions();
    if (opts.useLegacyAmmonia)
        return "/private/var/ammonia/core/tweaks";
    return "/opt/pluginplayground/tweaks";
}

static std::string tweakPath(const std::string &name) {
    return tweaksDir() + "/" + name;
}

static std::string tweakDisabledPath(const std::string &name) {
    return tweaksDir() + "/" + name + ".disabled";
}

static std::string tweakOptionsPath(const std::string &name) {
    return tweaksDir() + "/" + name + ".options";
}

static bool endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<TweakData> scanTweaks() {
    Options opts = loadOptions();
    std::vector<TweakData> result;

    // Migration from legacy .dylib.disabled files
    bool needsMigration = false;
    {
        DIR *d = opendir(tweaksDir().c_str());
        if (!d) return result;
        struct dirent *e;
        while ((e = readdir(d)) != nullptr) {
            if (endsWith(std::string(e->d_name), ".dylib.disabled")) {
                needsMigration = true;
                break;
            }
        }
        closedir(d);
    }

    if (needsMigration) {
        // Build enabledTweaks from non-disabled .dylib files
        DIR *d = opendir(tweaksDir().c_str());
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != nullptr) {
                std::string name(e->d_name);
                if (endsWith(name, ".dylib") && !endsWith(name, ".dylib.disabled")) {
                    std::string disabledPath = tweaksDir() + "/" + name + ".disabled";
                    if (access(disabledPath.c_str(), F_OK) != 0)
                        opts.enabledTweaks.push_back(name);
                }
            }
            closedir(d);
        }
        // Remove all .dylib.disabled files
        DIR *d2 = opendir(tweaksDir().c_str());
        if (d2) {
            struct dirent *e;
            while ((e = readdir(d2)) != nullptr) {
                std::string name(e->d_name);
                if (endsWith(name, ".dylib.disabled"))
                    unlink((tweaksDir() + "/" + name).c_str());
            }
            closedir(d2);
        }
        saveOptions(opts);
    }

    // Normal scan: list .dylib files, cross-reference with enabledTweaks
    DIR *dir = opendir(tweaksDir().c_str());
    if (!dir) return result;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (endsWith(name, ".dylib") && !endsWith(name, ".dylib.disabled")) {
            bool enabled = std::find(opts.enabledTweaks.begin(), opts.enabledTweaks.end(), name) != opts.enabledTweaks.end();
            result.push_back({name, !enabled});
        }
    }
    closedir(dir);
    return result;
}

static bool isTweakSafe(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    if (st.st_uid != 0) return false;
    if (st.st_mode & (S_IWGRP | S_IWOTH)) return false;
    return true;
}

bool toggleTweak(const std::string &name) {
    std::string base = name;
    if (endsWith(base, ".disabled"))
        base = base.substr(0, base.size() - 9);

    Options opts = loadOptions();
    auto it = std::find(opts.enabledTweaks.begin(), opts.enabledTweaks.end(), base);
    if (it != opts.enabledTweaks.end()) {
        opts.enabledTweaks.erase(it);
    } else {
        std::string fullPath = tweaksDir() + "/" + base;
        if (!isTweakSafe(fullPath))
            return false;
        opts.enabledTweaks.push_back(base);
    }
    return saveOptions(opts);
}

static bool runCommand(const char *cmd, const char *const argv[]) {
    pid_t pid;
    int r = posix_spawn(&pid, cmd, nullptr, nullptr,
                        (char *const *)argv, nullptr);
    if (r != 0) return false;
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool hasDeveloperTools() {
    const char *argv[] = {"/usr/bin/xcode-select", "-p", nullptr};
    return runCommand("/usr/bin/xcode-select", argv);
}

static bool is_safe_tweak_name(const std::string &name) {
    if (name.empty() || name.size() > 255) return false;
    for (char c : name) {
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_')
            return false;
    }
    return true;
}

bool packageTweak(const std::string &name) {
    if (!is_safe_tweak_name(name)) return false;

    std::string dylibPath = tweakPath(name);
    FILE *f = fopen(dylibPath.c_str(), "rb");
    if (!f) {
        dylibPath = tweakDisabledPath(name);
        f = fopen(dylibPath.c_str(), "rb");
    }
    if (!f) return false;
    fclose(f);

    std::string staging = "/tmp/plugintweak_" + name;
    std::string tweakDest = staging + "/opt/pluginplayground/tweaks";

    const char *mkdirArgs[] = {"/bin/mkdir", "-p", tweakDest.c_str(), nullptr};
    if (!runCommand("/bin/mkdir", mkdirArgs)) return false;

    std::string destFile = tweakDest + "/" + name;
    const char *cpArgs[] = {"/bin/cp", dylibPath.c_str(), destFile.c_str(), nullptr};
    if (!runCommand("/bin/cp", cpArgs)) return false;

    std::string pkgName = "/tmp/" + name + ".pkg";
    std::string pkgIdent = "com.pluginplayground.tweak." + name;
    const char *pkgbuildArgs[] = {
        "/usr/bin/pkgbuild", "--root", staging.c_str(),
        "--identifier", pkgIdent.c_str(),
        "--version", "1.0.0",
        "--install-location", "/",
        pkgName.c_str(), nullptr};
    bool ok = runCommand("/usr/bin/pkgbuild", pkgbuildArgs);

    const char *rmArgs[] = {"/bin/rm", "-rf", staging.c_str(), nullptr};
    runCommand("/bin/rm", rmArgs);
    return ok;
}

static CFStringRef strToCF(const std::string &s) {
    return CFStringCreateWithCString(kCFAllocatorDefault, s.c_str(), kCFStringEncodingUTF8);
}

static std::string cfToStr(CFStringRef s) {
    char buf[4096];
    if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
        return buf;
    return {};
}

TweakOptions loadTweakOptions(const std::string &name) {
    TweakOptions opts;
    std::string path = tweakOptionsPath(name);
    CFDataRef data = fileRead(path.c_str());
    if (!data) return opts;

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
    CFRelease(data);
    if (!plist) return opts;
    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return opts;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;

    auto readArray = [&](CFStringRef key, std::vector<std::string> &out) {
        CFArrayRef arr = (CFArrayRef)CFDictionaryGetValue(dict, key);
        if (!arr || CFGetTypeID(arr) != CFArrayGetTypeID()) return;
        CFIndex count = CFArrayGetCount(arr);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef s = (CFStringRef)CFArrayGetValueAtIndex(arr, i);
            if (s && CFGetTypeID(s) == CFStringGetTypeID())
                out.push_back(cfToStr(s));
        }
    };

    readArray(CFSTR("blacklistedApps"), opts.blacklistedApps);
    readArray(CFSTR("frameworkDependencies"), opts.frameworkDependencies);

    CFRelease(dict);
    return opts;
}

bool saveTweakOptions(const std::string &name, const TweakOptions &opts) {
    std::string path = tweakOptionsPath(name);

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    auto writeArray = [&](CFStringRef key, const std::vector<std::string> &items) {
        CFMutableArrayRef arr = CFArrayCreateMutable(kCFAllocatorDefault, items.size(), &kCFTypeArrayCallBacks);
        for (const auto &item : items) {
            CFStringRef s = strToCF(item);
            CFArrayAppendValue(arr, s);
            CFRelease(s);
        }
        CFDictionarySetValue(dict, key, arr);
        CFRelease(arr);
    };

    writeArray(CFSTR("blacklistedApps"), opts.blacklistedApps);
    writeArray(CFSTR("frameworkDependencies"), opts.frameworkDependencies);

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    CFRelease(dict);

    if (!data) return false;
    bool ok = fileWrite(path.c_str(), data);
    CFRelease(data);
    return ok;
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

bool ensurePermissions() {
    std::string dir = tweaksDir();
    if (access(dir.c_str(), R_OK | W_OK) == 0)
        return true;

    std::string script =
        "display dialog \"Plugin Playground needs permission to write to:\\n"
        + dir + "\\n\\n"
        "Click Fix to authenticate and fix permissions.\" "
        "buttons {\"Exit\", \"Fix\"} default button \"Fix\" with icon caution";

    pid_t dialogPid;
    const char *dialogArgs[] = {"/usr/bin/osascript", "-e", script.c_str(), nullptr};
    int r = posix_spawn(&dialogPid, "/usr/bin/osascript", nullptr, nullptr,
                        (char *const *)dialogArgs, nullptr);
    if (r != 0) return false;
    int status;
    waitpid(dialogPid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return false;

    return runPrivilegedScript(
        "do shell script \""
        "mkdir -p /opt/pluginplayground/tweaks && "
        "chown root:wheel /opt/pluginplayground && "
        "chmod 755 /opt/pluginplayground && "
        "chmod 755 /opt/pluginplayground/tweaks"
        "\" with administrator privileges");
}

SipStatus checkSipStatus() {
    pid_t pid;
    const char *args[] = {"/usr/bin/csrutil", "status", nullptr};
    int pipefd[2];
    if (pipe(pipefd) < 0) return SipStatus::Unknown;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);

    int r = posix_spawn(&pid, "/usr/bin/csrutil", &actions, nullptr,
                        (char *const *)args, nullptr);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (r != 0) { close(pipefd[0]); return SipStatus::Unknown; }

    char buffer[256];
    std::string result;
    ssize_t n;
    while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        result += buffer;
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (result.find("System Integrity Protection status: disabled.") != std::string::npos)
        return SipStatus::Disabled;
    if (result.find("Debugging Restrictions: disabled") != std::string::npos)
        return SipStatus::PartiallyDisabled;
    if (result.find("System Integrity Protection status: enabled.") != std::string::npos)
        return SipStatus::Enabled;
    return SipStatus::Unknown;
}

std::string sipStatusToString(SipStatus status) {
    switch (status) {
        case SipStatus::Enabled: return "Enabled";
        case SipStatus::Disabled: return "Disabled";
        case SipStatus::PartiallyDisabled: return "Partially Disabled (Debugging Restrictions Off)";
        default: return "Unknown";
    }
}

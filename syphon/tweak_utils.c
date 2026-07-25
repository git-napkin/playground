#include "tweak_utils.h"
#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <mach-o/dyld.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

bool path_ends_with(const char *path, const char *name) {
    if (!path || !name) return false;
    size_t path_len = strlen(path);
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > path_len) return false;
    return (strncmp(path + path_len - name_len, name, name_len) == 0) &&
           (path_len == name_len || path[path_len - name_len - 1] == '/');
}

bool path_matches_entry(const char *path, const char *entry) {
    if (!path || !entry || entry[0] == '\0') return false;
    if (strchr(entry, '/') != NULL)
        return (strcmp(path, entry) == 0);
    return path_ends_with(path, entry);
}

bool is_safe_filename(const char *name) {
    if (!name || !*name) return false;
    return strstr(name, "..") == NULL && strchr(name, '/') == NULL;
}

bool check_file_read(FILE *f, void *buf, size_t len) {
    return fread(buf, 1, len, f) == len;
}

uint32_t swap32_if(uint32_t val, bool swap) {
    return swap ? OSSwapBigToHostInt32(val) : val;
}

bool macho_has_framework(const char *base, size_t size, const char *framework) {
    (void)size;
    uint32_t magic = *(const uint32_t *)base;
    bool swap = (magic == MH_CIGAM_64 || magic == MH_CIGAM);
    uint32_t ncmds;
    const struct load_command *cmds;

    if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
        const struct mach_header_64 *mh = (const struct mach_header_64 *)base;
        ncmds = swap ? OSSwapBigToHostInt32(mh->ncmds) : mh->ncmds;
        cmds = (const struct load_command *)(base + sizeof(struct mach_header_64));
    } else if (magic == MH_MAGIC || magic == MH_CIGAM) {
        const struct mach_header *mh = (const struct mach_header *)base;
        ncmds = swap ? OSSwapBigToHostInt32(mh->ncmds) : mh->ncmds;
        cmds = (const struct load_command *)(base + sizeof(struct mach_header));
    } else {
        return false;
    }

    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "/%s.framework/", framework);
    size_t plen = strlen(pattern);

    const struct load_command *cursor = cmds;
    for (uint32_t i = 0; i < ncmds; i++) {
        uint32_t cmd = swap ? OSSwapBigToHostInt32(cursor->cmd) : cursor->cmd;
        uint32_t cmdsize = swap ? OSSwapBigToHostInt32(cursor->cmdsize)
                                : cursor->cmdsize;
        if (cmd == LC_LOAD_DYLIB || cmd == LC_LOAD_WEAK_DYLIB) {
            const struct dylib_command *dc =
                (const struct dylib_command *)cursor;
            uint32_t name_offset = swap
                                       ? OSSwapBigToHostInt32(dc->dylib.name.offset)
                                       : dc->dylib.name.offset;
            const char *dylib_path = (const char *)cursor + name_offset;
            if (strstr(dylib_path, pattern))
                return true;
        }
        cursor = (const struct load_command *)((const char *)cursor + cmdsize);
    }
    return false;
}

bool exe_links_to_framework(const char *exe_path, const char *framework) {
    int fd = open(exe_path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return false;
    }

    size_t size = (size_t)st.st_size;
    void *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return false;

    bool found = false;
    uint32_t magic = *(const uint32_t *)mapped;

    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        const struct fat_header *fh = (const struct fat_header *)mapped;
        uint32_t narch = OSSwapBigToHostInt32(fh->nfat_arch);
        const struct fat_arch *archs =
            (const struct fat_arch *)((const char *)mapped +
                                      sizeof(struct fat_header));
        for (uint32_t i = 0; i < narch; i++) {
            uint32_t offset = OSSwapBigToHostInt32(archs[i].offset);
            if (macho_has_framework((const char *)mapped + offset,
                                    size - offset, framework)) {
                found = true;
                break;
            }
        }
    } else {
        found = macho_has_framework((const char *)mapped, size, framework);
    }

    munmap(mapped, size);
    return found;
}

bool check_list_match(const char *path, const char *exe) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    bool matched = false;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '#')
            continue;
        if (path_matches_entry(exe, p)) {
            matched = true;
            break;
        }
    }
    fclose(f);
    return matched;
}

bool is_tweak_safe(const char *full_path) {
    struct stat st;
    if (stat(full_path, &st) != 0) return false;
    if (st.st_uid != 0) return false;
    if (st.st_mode & (S_IWGRP | S_IWOTH)) return false;
    return true;
}

bool should_load_tweak(const char *dir, const char *name, const char *exe) {
    if (!is_safe_filename(name)) return false;

    char wl[PATH_MAX], bl[PATH_MAX];
    snprintf(wl, sizeof(wl), "%s/%s.whitelist", dir, name);
    snprintf(bl, sizeof(bl), "%s/%s.blacklist", dir, name);

    if (access(wl, F_OK) == 0)
        return check_list_match(wl, exe);

    if (access(bl, F_OK) == 0)
        return !check_list_match(bl, exe);

    return false;
}

char *get_exe_path(void) {
    uint32_t bufsize = 0;
    _NSGetExecutablePath(NULL, &bufsize);
    char *path = malloc(bufsize);
    if (!path) return NULL;
    _NSGetExecutablePath(path, &bufsize);
    return path;
}

bool check_dylib_options(const char *dir, const char *name, const char *exe) {
    char optpath[PATH_MAX];
    snprintf(optpath, sizeof(optpath), "%s/%s.options", dir, name);

    FILE *f = fopen(optpath, "rb");
    if (!f) return true;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return true; }
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)len);
    if (!buf) { fclose(f); return true; }
    if (!check_file_read(f, buf, (size_t)len)) {
        free(buf);
        fclose(f);
        return true;
    }
    fclose(f);

    CFDataRef cfData = CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault, (const UInt8 *)buf, (CFIndex)len,
        kCFAllocatorNull);
    if (!cfData) { free(buf); return true; }

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, cfData, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(cfData);
    free(buf);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist) CFRelease(plist);
        return true;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;
    bool should_load = true;

    CFArrayRef frameworks = (CFArrayRef)CFDictionaryGetValue(
        dict, CFSTR("frameworkDependencies"));
    if (should_load && frameworks &&
        CFGetTypeID(frameworks) == CFArrayGetTypeID() &&
        CFArrayGetCount(frameworks) > 0) {
        should_load = false;
        CFIndex count = CFArrayGetCount(frameworks);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef str =
                (CFStringRef)CFArrayGetValueAtIndex(frameworks, i);
            if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
                char fname[256];
                CFStringGetCString(str, fname, sizeof(fname),
                                   kCFStringEncodingUTF8);
                if (exe_links_to_framework(exe, fname)) {
                    should_load = true;
                    break;
                }
            }
        }
    }

    CFArrayRef blacklisted = (CFArrayRef)CFDictionaryGetValue(
        dict, CFSTR("blacklistedApps"));
    if (should_load && blacklisted &&
        CFGetTypeID(blacklisted) == CFArrayGetTypeID() &&
        CFArrayGetCount(blacklisted) > 0) {
        const char *base = strrchr(exe, '/');
        base = base ? base + 1 : exe;
        CFIndex count = CFArrayGetCount(blacklisted);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef str =
                (CFStringRef)CFArrayGetValueAtIndex(blacklisted, i);
            if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
                char appname[256];
                CFStringGetCString(str, appname, sizeof(appname),
                                   kCFStringEncodingUTF8);
                if (path_matches_entry(exe, appname) ||
                    path_matches_entry(base, appname)) {
                    should_load = false;
                    break;
                }
            }
        }
    }

    CFRelease(dict);
    return should_load;
}

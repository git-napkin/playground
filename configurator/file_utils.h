#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
#include <vector>

static inline CFDataRef fileRead(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return nullptr; }
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data((size_t)len);
    if (fread(data.data(), 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        return nullptr;
    }
    fclose(f);
    return CFDataCreate(kCFAllocatorDefault, data.data(), (CFIndex)len);
}

static inline bool fileWrite(const char *path, CFDataRef data) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f) == (size_t)CFDataGetLength(data);
    fclose(f);
    return ok;
}

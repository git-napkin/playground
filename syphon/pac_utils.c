#include "pac_utils.h"
#include <fcntl.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

extern const CFStringRef kSecCodeSignerIdentity;
extern const CFStringRef kSecCodeSignerEntitlements;
extern const CFStringRef kSecCodeSignerDigestAlgorithm;
typedef struct __SecCodeSigner *SecCodeSignerRef;
extern OSStatus SecCodeSignerCreate(CFDictionaryRef parameters, SecCSFlags flags, SecCodeSignerRef *signer);
extern OSStatus SecCodeSignerAddSignatureWithErrors(SecCodeSignerRef signer, SecStaticCodeRef code, SecCSFlags flags, CFErrorRef *errors);

#define CPU_SUBTYPE_ARM64_MASK 0x00ffffff

static bool is_arm64e_subtype(uint32_t cpusubtype) {
    return (cpusubtype & CPU_SUBTYPE_ARM64_MASK) == CPU_SUBTYPE_ARM64E;
}

static void depacify_binary(uint8_t *d, size_t s) {
    uint32_t m = *(uint32_t *)d;
    if (m == MH_MAGIC_64) {
        struct mach_header_64 *h = (struct mach_header_64 *)d;
        if (h->cputype == CPU_TYPE_ARM64 && is_arm64e_subtype(h->cpusubtype))
            h->cpusubtype = 0;
    } else if (m == FAT_MAGIC || m == FAT_CIGAM) {
        bool swap = (m == FAT_CIGAM);
        struct fat_header *fh = (struct fat_header *)d;
        uint32_t n = swap ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        struct fat_arch *as = (struct fat_arch *)(d + sizeof(*fh));
        for (uint32_t i = 0; i < n; i++) {
            uint32_t off = swap ? __builtin_bswap32(as[i].offset) : as[i].offset;
            uint32_t t = swap ? __builtin_bswap32(as[i].cputype) : as[i].cputype;
            uint32_t sbt = swap ? __builtin_bswap32(as[i].cpusubtype) : as[i].cpusubtype;
            if (t == CPU_TYPE_ARM64 && is_arm64e_subtype(sbt)) {
                depacify_binary((uint8_t *)d + off, s - off);
                as[i].cpusubtype = swap ? __builtin_bswap32(0) : 0;
            }
        }
    }
}

static bool strip_code_signature_thin(uint8_t *data, size_t size) {
    if (*(uint32_t *)data != MH_MAGIC_64)
        return true;

    struct mach_header_64 *header = (struct mach_header_64 *)data;
    uint8_t *src = (uint8_t *)(data + sizeof(*header));
    uint8_t *dst = src;
    uint32_t new_ncmds = 0;
    uint32_t new_sizeofcmds = 0;
    uint32_t freed = 0;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        struct load_command *lc = (struct load_command *)src;
        uint32_t cmdsize = lc->cmdsize;

        if (lc->cmd == LC_CODE_SIGNATURE) {
            freed += cmdsize;
        } else {
            if (dst != src)
                memmove(dst, src, cmdsize);
            dst += cmdsize;
            new_ncmds++;
            new_sizeofcmds += cmdsize;
        }
        src += cmdsize;
    }

    if (freed > 0) {
        memset(data + sizeof(*header) + new_sizeofcmds, 0, freed);
        header->ncmds = new_ncmds;
        header->sizeofcmds = new_sizeofcmds;
    }

    (void)size;
    return true;
}

bool strip_code_signature_file(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0)
        return false;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }

    uint8_t *data = (uint8_t *)mmap(NULL, (size_t)st.st_size,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    bool ok = true;
    uint32_t magic = *(uint32_t *)data;
    if (magic == MH_MAGIC_64) {
        ok = strip_code_signature_thin(data, (size_t)st.st_size);
    } else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        bool swap = (magic == FAT_CIGAM);
        struct fat_header *fh = (struct fat_header *)data;
        uint32_t nfat = swap ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        struct fat_arch *arches = (struct fat_arch *)(data + sizeof(*fh));
        for (uint32_t i = 0; i < nfat && ok; i++) {
            uint32_t offset = swap ? __builtin_bswap32(arches[i].offset) : arches[i].offset;
            if (*(uint32_t *)(data + offset) == MH_MAGIC_64) {
                ok = strip_code_signature_thin(data + offset, (size_t)st.st_size - offset);
            }
        }
    }

    if (msync(data, (size_t)st.st_size, MS_SYNC) != 0)
        ok = false;

    munmap(data, (size_t)st.st_size);
    close(fd);
    return ok;
}

bool sign_file(const char *path, void *entitlements_blob) {
    CFMutableDictionaryRef params = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!params)
        return false;

    CFDictionaryAddValue(params, kSecCodeSignerIdentity, kCFNull);

    if (entitlements_blob)
        CFDictionaryAddValue(params, kSecCodeSignerEntitlements, entitlements_blob);

    int digest_value = kSecCodeSignatureHashSHA256;
    CFNumberRef digest_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &digest_value);
    if (!digest_number) {
        CFRelease(params);
        return false;
    }
    const void *digests[] = {digest_number};
    CFArrayRef digest_array = CFArrayCreate(kCFAllocatorDefault, digests, 1, &kCFTypeArrayCallBacks);
    CFRelease(digest_number);
    if (!digest_array) {
        CFRelease(params);
        return false;
    }
    CFDictionaryAddValue(params, kSecCodeSignerDigestAlgorithm, digest_array);
    CFRelease(digest_array);

    SecCodeSignerRef signer = NULL;
    OSStatus status = SecCodeSignerCreate(params, kSecCSDefaultFlags, &signer);
    CFRelease(params);
    if (status != errSecSuccess)
        return false;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)path, strlen(path), false);
    if (!url) {
        CFRelease(signer);
        return false;
    }

    SecStaticCodeRef static_code = NULL;
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &static_code);
    CFRelease(url);
    if (status != errSecSuccess) {
        CFRelease(signer);
        return false;
    }

    CFErrorRef error = NULL;
    status = SecCodeSignerAddSignatureWithErrors(signer, static_code, kSecCSDefaultFlags, &error);
    CFRelease(signer);
    CFRelease(static_code);
    if (error)
        CFRelease(error);

    return status == errSecSuccess;
}

bool depacify_file_in_place(const char *file_path) {
    int fd = open(file_path, O_RDWR);
    if (fd < 0)
        return false;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }

    size_t size = (size_t)st.st_size;
    uint8_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    depacify_binary(data, size);

    if (msync(data, size, MS_SYNC) != 0) {
        munmap(data, size);
        close(fd);
        return false;
    }

    if (fsync(fd) != 0) {
        munmap(data, size);
        close(fd);
        return false;
    }

    munmap(data, size);
    close(fd);
    return true;
}


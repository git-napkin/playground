#include "bundle_copy.h"
#include "pac_utils.h"
#include "log.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

bool path_is_bundle(const char *path) {
    if (!path) return false;
    const char *dot = strstr(path, ".app");
    while (dot) {
        char c = dot[4];
        if (c == '/' || c == '\0')
            return true;
        dot = strstr(dot + 4, ".app");
    }
    return false;
}

bool get_bundle_executable_path(const char *bundle_path, char *exec_path, size_t exec_path_size) {
    CFURLRef bundle_url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)bundle_path,
        (CFIndex)strlen(bundle_path), true);
    if (!bundle_url)
        return false;

    CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, bundle_url);
    CFRelease(bundle_url);
    if (!bundle)
        return false;

    CFURLRef exec_url = CFBundleCopyExecutableURL(bundle);
    CFRelease(bundle);
    if (!exec_url)
        return false;

    bool result = CFURLGetFileSystemRepresentation(
        exec_url, true, (UInt8 *)exec_path, (CFIndex)exec_path_size);
    CFRelease(exec_url);
    return result;
}

typedef struct DirPair {
    char *src;
    char *dst;
    struct DirPair *next;
} DirPair;

static bool copy_dir_iterative(const char *root_src, const char *root_dst) {
    DirPair *stack = NULL;

    DirPair *first = malloc(sizeof(DirPair));
    if (!first)
        return false;
    first->src = strdup(root_src);
    first->dst = strdup(root_dst);
    first->next = NULL;
    if (!first->src || !first->dst) {
        free(first->src);
        free(first->dst);
        free(first);
        return false;
    }
    stack = first;

    bool ok = true;
    while (stack && ok) {
        DirPair *cur = stack;
        stack = stack->next;

        if (mkdir(cur->dst, 0755) != 0) {
            log_error("[copy_dir] mkdir failed: %s (%s)", cur->dst, strerror(errno));
            ok = false;
            free(cur->src);
            free(cur->dst);
            free(cur);
            break;
        }

        DIR *d = opendir(cur->src);
        if (!d) {
            log_error("[copy_dir] opendir failed: %s (%s)", cur->src, strerror(errno));
            ok = false;
            free(cur->src);
            free(cur->dst);
            free(cur);
            break;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && ok) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char *src_sub = NULL;
            char *dst_sub = NULL;
            if (asprintf(&src_sub, "%s/%s", cur->src, entry->d_name) < 0 ||
                asprintf(&dst_sub, "%s/%s", cur->dst, entry->d_name) < 0) {
                free(src_sub);
                free(dst_sub);
                ok = false;
                break;
            }

            struct stat st;
            if (lstat(src_sub, &st) != 0) {
                log_error("[copy_dir] lstat failed: %s (%s)", src_sub, strerror(errno));
                free(src_sub);
                free(dst_sub);
                ok = false;
                break;
            }

            if (S_ISDIR(st.st_mode)) {
                DirPair *pair = malloc(sizeof(DirPair));
                if (!pair) {
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }
                pair->src = src_sub;
                pair->dst = dst_sub;
                pair->next = stack;
                stack = pair;
            } else if (S_ISREG(st.st_mode)) {
                int src_fd = open(src_sub, O_RDONLY | O_NOFOLLOW);
                if (src_fd < 0) {
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }

                int dst_fd = open(dst_sub, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
                if (dst_fd < 0) {
                    close(src_fd);
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }

                char *buf = malloc(65536);
                if (!buf) {
                    close(src_fd);
                    close(dst_fd);
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }
                while (ok) {
                    ssize_t nr = read(src_fd, buf, 65536);
                    if (nr == 0)
                        break;
                    if (nr < 0) {
                        ok = false;
                        break;
                    }
                    ssize_t written = 0;
                    while (written < nr) {
                        ssize_t nw = write(dst_fd, buf + written, (size_t)(nr - written));
                        if (nw < 0) {
                            ok = false;
                            break;
                        }
                        written += nw;
                    }
                }
                free(buf);
                close(src_fd);
                close(dst_fd);
                free(src_sub);
                free(dst_sub);
            } else if (S_ISLNK(st.st_mode)) {
                char *link_buf = malloc(PATH_MAX);
                if (!link_buf) {
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }
                ssize_t len = readlink(src_sub, link_buf, PATH_MAX - 1);
                if (len < 0) {
                    free(link_buf);
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }
                link_buf[len] = '\0';
                if (symlink(link_buf, dst_sub) != 0) {
                    free(link_buf);
                    free(src_sub);
                    free(dst_sub);
                    ok = false;
                    break;
                }
                free(link_buf);
                free(src_sub);
                free(dst_sub);
            } else {
                free(src_sub);
                free(dst_sub);
            }
        }

        closedir(d);
        free(cur->src);
        free(cur->dst);
        free(cur);
    }

    while (stack) {
        DirPair *tmp = stack;
        stack = stack->next;
        free(tmp->src);
        free(tmp->dst);
        free(tmp);
    }

    return ok;
}

bool copy_dir_recursive(const char *src_path, const char *dst_path) {
    return copy_dir_iterative(src_path, dst_path);
}

bool resign_bundle(const char *bundle_path) {
    char exec_path[PATH_MAX];
    if (!get_bundle_executable_path(bundle_path, exec_path, sizeof(exec_path)))
        return false;
    if (!strip_code_signature_file(exec_path))
        return false;
    return sign_file(exec_path, NULL);
}

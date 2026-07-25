#include "exe.h"
#include "bundle_copy.h"
#include "pac_utils.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *getready_process(const char *path) {
    if (!path_is_bundle(path))
        return strdup(path);

    char bundle_root[PATH_MAX];
    snprintf(bundle_root, sizeof(bundle_root), "%s", path);
    char *app_ext = strstr(bundle_root, ".app");
    if (app_ext)
        app_ext[4] = '\0';

    const char *bundle_name = strrchr(bundle_root, '/');
    bundle_name = bundle_name ? bundle_name + 1 : bundle_root;

    char runtime_apps_dir[PATH_MAX];
    char dst_bundle_path[PATH_MAX];
    snprintf(runtime_apps_dir, sizeof(runtime_apps_dir), "/tmp/RuntimeApplications");
    snprintf(dst_bundle_path, sizeof(dst_bundle_path), "%s/%s", runtime_apps_dir, bundle_name);

    log_info("[bootstrap] processing bundle: %s", bundle_root);

    struct stat st;
    if (stat(dst_bundle_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        char bundle_exec_tmp[PATH_MAX];
        log_info("[bootstrap] bundle already exists at: %s", dst_bundle_path);
        if (get_bundle_executable_path(dst_bundle_path, bundle_exec_tmp, sizeof(bundle_exec_tmp)))
            return strdup(bundle_exec_tmp);
    }

    if (mkdir(runtime_apps_dir, 0755) != 0 && errno != EEXIST) {
        log_error("[bootstrap] failed to create RuntimeApplications dir: %s", strerror(errno));
        return strdup(path);
    }

    log_info("[bootstrap] copying bundle to: %s", dst_bundle_path);
    if (!copy_dir_recursive(bundle_root, dst_bundle_path)) {
        log_error("[bootstrap] failed to copy bundle to: %s", dst_bundle_path);
        return strdup(path);
    }

    char bundle_exec_tmp[PATH_MAX];
    if (!get_bundle_executable_path(dst_bundle_path, bundle_exec_tmp, sizeof(bundle_exec_tmp))) {
        log_error("[bootstrap] failed to get executable path for: %s", dst_bundle_path);
        return strdup(path);
    }

    log_info("[bootstrap] depacifying executable: %s", bundle_exec_tmp);
    if (!depacify_file_in_place(bundle_exec_tmp)) {
        log_error("Warning: failed to depacify bundle executable");
        return strdup(bundle_exec_tmp);
    }

    log_info("[bootstrap] resigning bundle: %s", dst_bundle_path);
    if (!resign_bundle(dst_bundle_path))
        log_error("Warning: failed to resign bundle, continuing anyway");

    log_info("[bootstrap] using depacified bundle");
    return strdup(bundle_exec_tmp);
}

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/syslimits.h>

bool path_is_bundle(const char *path);
bool get_bundle_executable_path(const char *bundle_path, char *exec_path, size_t exec_path_size);
bool copy_dir_recursive(const char *src_path, const char *dst_path);
bool copy_bundle_to_tmp(const char *bundle_path, char *tmp_path, size_t tmp_path_size);
bool resign_bundle(const char *bundle_path);

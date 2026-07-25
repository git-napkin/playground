#include "../syphon/envbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_pass = 0, tests_fail = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS do { printf("PASS\n"); tests_pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_len_null(void) {
    TEST("envbuf_len(NULL)");
    ASSERT(envbuf_len(NULL) == 0, "expected 0");
    PASS;
}

static void test_len_empty(void) {
    TEST("envbuf_len({NULL})");
    const char *env[] = {NULL};
    ASSERT(envbuf_len(env) == 1, "expected 1 for {NULL}");
    PASS;
}

static void test_len_three(void) {
    TEST("envbuf_len({a,b,c})");
    const char *env[] = {"A=1", "B=2", "C=3", NULL};
    ASSERT(envbuf_len(env) == 4, "expected 4");
    PASS;
}

static void test_mutcopy_null(void) {
    TEST("envbuf_mutcopy(NULL)");
    char **c = envbuf_mutcopy(NULL);
    ASSERT(c == NULL, "expected NULL");
    PASS;
}

static void test_mutcopy_basic(void) {
    TEST("envbuf_mutcopy basic");
    const char *env[] = {"A=1", "B=2", NULL};
    char **c = envbuf_mutcopy((const char **)env);
    ASSERT(c != NULL, "got NULL");
    ASSERT(strcmp(c[0], "A=1") == 0, "c[0] mismatch");
    ASSERT(strcmp(c[1], "B=2") == 0, "c[1] mismatch");
    ASSERT(c[2] == NULL, "c[2] not NULL");
    envbuf_free(c);
    PASS;
}

static void test_find(void) {
    TEST("envbuf_find");
    const char *env[] = {"A=1", "B=2", "CC=3", NULL};
    ASSERT(envbuf_find(env, "A") == 0, "A should be at 0");
    ASSERT(envbuf_find(env, "B") == 1, "B should be at 1");
    ASSERT(envbuf_find(env, "CC") == 2, "CC should be at 2");
    ASSERT(envbuf_find(env, "C") == -1, "C should not match CC=");
    ASSERT(envbuf_find(env, "D") == -1, "D not found");
    PASS;
}

static void test_getenv(void) {
    TEST("envbuf_getenv");
    const char *env[] = {"A=1", "B=hello", NULL};
    const char *v = envbuf_getenv(env, "A");
    ASSERT(v != NULL && strcmp(v, "1") == 0, "A wrong");
    v = envbuf_getenv(env, "B");
    ASSERT(v != NULL && strcmp(v, "hello") == 0, "B wrong");
    v = envbuf_getenv(env, "C");
    ASSERT(v == NULL, "C should be NULL");
    PASS;
}

static void test_setenv_new(void) {
    TEST("envbuf_setenv new");
    char **env = envbuf_mutcopy((const char *[]){"A=1", NULL});
    env = envbuf_setenv(env, "B", "2");
    ASSERT(env != NULL, "got NULL");
    const char *v = envbuf_getenv((const char **)env, "B");
    ASSERT(v != NULL && strcmp(v, "2") == 0, "B wrong");
    ASSERT(envbuf_getenv((const char **)env, "A") != NULL, "A missing");
    envbuf_free(env);
    PASS;
}

static void test_setenv_replace(void) {
    TEST("envbuf_setenv replace");
    char **env = envbuf_mutcopy((const char *[]){"A=old", NULL});
    env = envbuf_setenv(env, "A", "new");
    ASSERT(env != NULL, "got NULL");
    const char *v = envbuf_getenv((const char **)env, "A");
    ASSERT(v != NULL && strcmp(v, "new") == 0, "A not updated");
    envbuf_free(env);
    PASS;
}

static void test_unsetenv(void) {
    TEST("envbuf_unsetenv");
    char **env = envbuf_mutcopy((const char *[]){"A=1", "B=2", "C=3", NULL});
    env = envbuf_unsetenv(env, "B");
    ASSERT(env != NULL, "got NULL");
    ASSERT(envbuf_find((const char **)env, "B") == -1, "B still present");
    ASSERT(strcmp(env[0], "A=1") == 0, "order broken at 0");
    ASSERT(strcmp(env[1], "C=3") == 0, "order broken at 1");
    ASSERT(env[2] == NULL, "not NULL-terminated");
    envbuf_free(env);
    PASS;
}

int main(void) {
    printf("envbuf tests:\n");
    test_len_null();
    test_len_empty();
    test_len_three();
    test_mutcopy_null();
    test_mutcopy_basic();
    test_find();
    test_getenv();
    test_setenv_new();
    test_setenv_replace();
    test_unsetenv();

    printf("\n%d passed, %d failed\n", tests_pass, tests_fail);
    return tests_fail > 0 ? 1 : 0;
}

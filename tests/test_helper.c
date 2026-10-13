#include "helper.h"
#include "softgred_config.h"

static void test_processes(void)
{
    const char *ok[] = {"true", NULL};
    const char *bad[] = {"false", NULL};
    const char *missing[] = {"softgre-command-does-not-exist", NULL};
    g_assert_cmpint(helper_exec(ok), ==, 0);
    g_assert_cmpint(helper_exec(bad), ==, -1);
    g_assert_cmpint(helper_exec(missing), ==, -1);
    /* A blocked daemon SIGTERM must not survive into its command children. */
    sigset_t mask, saved;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    g_assert_cmpint(sigprocmask(SIG_BLOCK, &mask, &saved), ==, 0);
    const char *terminated[] = {"sh", "-c", "kill -TERM $$; exit 0", NULL};
    g_assert_cmpint(helper_exec(terminated), ==, -1);
    g_assert_cmpint(sigprocmask(SIG_SETMASK, &saved, NULL), ==, 0);
    const char *timeout[] = {"sleep", "5", NULL};
    g_assert_cmpint(helper_exec(timeout), ==, -1);
}

static void test_no_shell(void)
{
    char *path;
    int fd = g_file_open_tmp("softgre-exec-XXXXXX", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    close(fd);
    unlink(path);
    char *text = g_strdup_printf("$(touch %s); touch %s", path, path);
    const char *args[] = {"printf", "%s", text, NULL};
    g_assert_cmpint(helper_exec(args), ==, 0);
    g_assert_cmpint(access(path, F_OK), ==, -1);
    g_free(text);
    g_free(path);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/helper/process-failures-and-timeout", test_processes);
    g_test_add_func("/helper/explicit-arguments", test_no_shell);
    return g_test_run();
}

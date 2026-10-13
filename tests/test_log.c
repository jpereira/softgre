/* File logging must preserve events across rotation and reject unsafe destinations. */
#include "log.h"
#include "softgred_config.h"

#include <glib/gstdio.h>

static char *contents(const char *path)
{
    char *text = NULL;
    g_assert_true(g_file_get_contents(path, &text, NULL, NULL));
    return text;
}

static void test_logging(void)
{
    char *directory = g_dir_make_tmp("softgre-log-XXXXXX", NULL);
    g_assert_nonnull(directory);
    char *path = g_build_filename(directory, "events.log", NULL);
    char *old = g_build_filename(directory, "events.log.1", NULL);
    char *unsafe = g_build_filename(directory, "unsafe", NULL);
    g_assert_cmpint(log_open(path), ==, 0);
    errno = EBUSY;
    D_INFO("First event remote=192.0.2.2\n");
    g_assert_cmpint(errno, ==, EBUSY);
    D_DEBUG1("hidden debug\n");
    D_WARNING("Warning event\n");
    char *text = contents(path);
    g_assert_nonnull(strstr(text, "level=INFO First event remote=192.0.2.2\n"));
    g_assert_nonnull(strstr(text, "level=WARNING Warning event\n"));
    g_assert_null(strstr(text, "hidden debug"));
    g_assert_true(g_regex_match_simple("^\\[[0-9]{4}-[0-9]{2}-[0-9]{2}T.*\\] pid=", text, 0, 0));
    g_free(text);
    struct stat st;
    g_assert_cmpint(stat(path, &st), ==, 0);
    g_assert_cmpuint(st.st_mode & 0777, ==, 0600);
    g_assert_cmpint(rename(path, old), ==, 0);
    D_INFO("Before reopen\n");
    g_assert_cmpint(log_open(path), ==, 0);
    D_INFO("After reopen\n");
    text = contents(old);
    g_assert_nonnull(strstr(text, "First event"));
    g_assert_nonnull(strstr(text, "Before reopen"));
    g_assert_null(strstr(text, "After reopen"));
    g_free(text);
    g_assert_cmpint(symlink(old, unsafe), ==, 0);
    g_assert_cmpint(log_open(unsafe), ==, -1);
    D_CRIT("Kept previous descriptor\n");
    text = contents(path);
    g_assert_nonnull(strstr(text, "After reopen"));
    g_assert_nonnull(strstr(text, "level=ERROR Kept previous descriptor"));
    g_free(text);
    g_assert_cmpint(unlink(unsafe), ==, 0);
    g_assert_cmpint(link(path, unsafe), ==, 0);
    g_assert_cmpint(log_open(unsafe), ==, -1);
    g_assert_cmpint(unlink(unsafe), ==, 0);
    g_assert_cmpint(mkfifo(unsafe, 0600), ==, 0);
    g_assert_cmpint(log_open(unsafe), ==, -1);
    g_assert_cmpint(unlink(unsafe), ==, 0);
    g_assert_cmpint(log_open(directory), ==, -1);
    g_assert_cmpint(log_open("relative.log"), ==, -1);
    log_close();
    g_assert_cmpint(log_open(path), ==, 0);
    softgred_config_ref()->dbg_mode = 1;
    D_DEBUG1("Visible debug\n");
    log_close();
    text = contents(path);
    g_assert_nonnull(strstr(text, "After reopen"));
    g_assert_nonnull(strstr(text, "level=DEBUG Visible debug"));
    g_free(text);
    unlink(path);
    unlink(old);
    rmdir(directory);
    g_free(path);
    g_free(old);
    g_free(unsafe);
    g_free(directory);
    softgred_config_release();
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/log/append-rotation-and-safety", test_logging);
    return g_test_run();
}

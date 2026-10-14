#include "helper.h"
#include "iface_ebtables.h"
#include "testing.h"

static unsigned calls, fail_call;
static char **commands;
static size_t commands_len, commands_cap;

static char *join_argv(const char *const argv[])
{
    size_t total = 1;
    for (unsigned i = 0; argv[i]; i++)
        total += strlen(argv[i]) + 1;
    char *out = xmalloc(total);
    out[0] = '\0';
    for (unsigned i = 0; argv[i]; i++) {
        if (i)
            strcat(out, " ");
        strcat(out, argv[i]);
    }
    return out;
}

int helper_exec(const char *const argv[])
{
    if (commands_len == commands_cap) {
        commands_cap = commands_cap ? commands_cap * 2 : 16;
        commands = xrealloc(commands, commands_cap * sizeof(*commands));
    }
    commands[commands_len++] = join_argv(argv);
    calls++;
    return calls == fail_call ? -1 : 0;
}

static void test_isolation(void)
{
    commands_len = 0;
    g_assert_cmpint(iface_ebtables_init(), ==, 0);
    unsigned installed = 0;
    fail_call = calls + 2;
    g_assert_cmpint(iface_ebtables_add_tunnel("sgre0", &installed), ==, -1);
    g_assert_cmpuint(installed, ==, 1);
    fail_call = 0;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &installed), ==, 0);
    g_assert_cmpuint(installed, ==, 0);
    g_assert_cmpint(iface_ebtables_add_tunnel("sgre0", &installed), ==, 0);
    g_assert_cmpint(iface_ebtables_allow("sgre0", "02:00:00:00:00:42"), ==, 0);
    fail_call = calls + 2;
    g_assert_cmpint(iface_ebtables_allow("sgre0", "02:00:00:00:00:43"), ==, -1);
    g_assert_nonnull(
        strstr(commands[commands_len - 1], "-D SOFTGRE_IN -i sgre0.+ -s 02:00:00:00:00:43"));
    fail_call = 0;
    fail_call = calls + 1;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &installed), ==, -1);
    g_assert_cmpuint(installed, ==, 1);
    fail_call = 0;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &installed), ==, 0);
    /* All bridge paths must be protected, including local gateway traffic. */
    for (unsigned hook = 1; hook <= 3; hook++) {
        installed = 0;
        fail_call = calls + hook;
        g_assert_cmpint(iface_ebtables_add_tunnel("sgre0", &installed), ==, -1);
        g_assert_cmpuint(installed, ==, (1u << (hook - 1)) - 1);
        fail_call = 0;
        g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &installed), ==, 0);
        g_assert_cmpuint(installed, ==, 0);
    }
    bool input = false, output = false;
    for (size_t i = 0; i < commands_len; i++) {
        input |= strstr(commands[i], "-t nat -I PREROUTING -i sgre0.+ -j SOFTGRE_IN") != NULL;
        output |= strstr(commands[i], "-I OUTPUT -o sgre0.+ -j SOFTGRE_OUT") != NULL;
    }
    g_assert_true(input);
    g_assert_true(output);
    unsigned before = calls;
    g_assert_cmpint(iface_ebtables_reset(), ==, 0);
    g_assert_cmpuint(calls - before, ==, 5);
    g_assert_cmpint(iface_ebtables_end(), ==, 0);
    before = calls;
    g_assert_cmpint(iface_ebtables_end(), ==, 0);
    g_assert_cmpuint(calls, ==, before);
    for (size_t i = 0; i < commands_len; i++) {
        const char *command = commands[i];
        g_assert_null(strstr(command, "-F FORWARD"));
        g_assert_null(strstr(command, "-P FORWARD"));
        if (strstr(command, "-i") || strstr(command, "-o"))
            g_assert_nonnull(strstr(command, "sgre0.+"));
    }
    for (size_t i = 0; i < commands_len; i++)
        free(commands[i]);
    free(commands);
    commands = NULL;
    commands_cap = 0;
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/firewall/isolation-and-partial-failure", test_isolation);
    return g_test_run();
}

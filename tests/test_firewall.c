#include "iface_ebtables.h"
#include "helper.h"
static unsigned calls, fail_call;
static GPtrArray *commands;
int helper_exec(const char *const argv[])
{
    char *command = g_strjoinv(" ", (char **)argv);
    g_ptr_array_add(commands, command);
    calls++;
    return calls == fail_call ? -1 : 0;
}
static void test_isolation(void)
{
    commands = g_ptr_array_new_with_free_func(g_free);
    g_assert_cmpint(iface_ebtables_init(), ==, 0);
    bool in = false, out = false;
    fail_call = calls + 2;
    g_assert_cmpint(iface_ebtables_add_tunnel("sgre0", &in, &out), ==, -1);
    g_assert_true(in); g_assert_false(out);
    fail_call = 0;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &in, &out), ==, 0);
    g_assert_false(in); g_assert_false(out);
    g_assert_cmpint(iface_ebtables_add_tunnel("sgre0", &in, &out), ==, 0);
    g_assert_cmpint(iface_ebtables_allow("sgre0", "02:00:00:00:00:42"), ==, 0);
    fail_call = calls + 2;
    g_assert_cmpint(iface_ebtables_allow("sgre0", "02:00:00:00:00:43"), ==, -1);
    g_assert_nonnull(strstr(g_ptr_array_index(commands, commands->len - 1),
                           "-D SOFTGRE_IN -i sgre0.+ -s 02:00:00:00:00:43"));
    fail_call = 0;
    fail_call = calls + 1;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &in, &out), ==, -1);
    g_assert_true(in); g_assert_false(out);
    fail_call = 0;
    g_assert_cmpint(iface_ebtables_del_tunnel("sgre0", &in, &out), ==, 0);
    unsigned before = calls;
    g_assert_cmpint(iface_ebtables_reset(), ==, 0);
    g_assert_cmpuint(calls - before, ==, 5);
    g_assert_cmpint(iface_ebtables_end(), ==, 0);
    before = calls;
    g_assert_cmpint(iface_ebtables_end(), ==, 0);
    g_assert_cmpuint(calls, ==, before);
    for (unsigned i = 0; i < commands->len; i++) {
        const char *command = g_ptr_array_index(commands, i);
        g_assert_null(strstr(command, "-F FORWARD"));
        g_assert_null(strstr(command, "-P FORWARD"));
        if (strstr(command, "-i") || strstr(command, "-o"))
            g_assert_nonnull(strstr(command, "sgre0.+"));
    }
    g_ptr_array_free(commands, true);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/firewall/isolation-and-partial-failure", test_isolation);
    return g_test_run();
}

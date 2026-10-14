/* Regression tests exercise parsing and state transitions without touching host networking. */
#include "general.h"
#include "iface_bridge.h"
#include "iface_ebtables.h"
#include "iface_gre.h"
#include "payload.h"
#include "provision.h"
#include "service.h"
#include "service_cmd.h"
#include "softgred_config.h"

static unsigned adds, deletes, allows;
static bool fail_gre, fail_bridge, fail_firewall, fail_delete;

int iface_gre_add(const char *n, const char *k, const char *d, const struct in_addr *l,
                  const struct in_addr *r)
{
    (void)n;
    (void)k;
    (void)d;
    (void)l;
    (void)r;
    adds++;
    return fail_gre ? -1 : 0;
}

int iface_gre_del(const char *n)
{
    (void)n;
    deletes++;
    errno = EPERM;
    return fail_delete ? -1 : 0;
}

int iface_bridge_attach(const char *n, const char *b, uint16_t v)
{
    (void)n;
    (void)b;
    (void)v;
    return fail_bridge ? -1 : 0;
}

int iface_ebtables_add_tunnel(const char *n, bool *in, bool *out)
{
    (void)n;
    *in = *out = true;
    return 0;
}

int iface_ebtables_del_tunnel(const char *n, bool *in, bool *out)
{
    (void)n;
    *in = *out = false;
    return 0;
}

int iface_ebtables_reset(void)
{
    return 0;
}

int iface_ebtables_revoke(const char *n, const char *m)
{
    (void)n;
    (void)m;
    return 0;
}

int iface_ebtables_allow(const char *n, const char *m)
{
    (void)n;
    (void)m;
    allows++;
    return fail_firewall ? -1 : 0;
}

static void write16(unsigned char *p, uint16_t x)
{
    p[0] = x >> 8;
    p[1] = x;
}

static size_t packet(unsigned char *p, unsigned options, unsigned outer_tags)
{
    memset(p, 0, 128);
    size_t offset = 14;
    write16(p + 12, outer_tags ? 0x8100 : 0x0800);
    for (unsigned i = 0; i < outer_tags; i++) {
        write16(p + offset, 123);
        write16(p + offset + 2, i + 1 < outer_tags ? 0x8100 : 0x0800);
        offset += 4;
    }
    unsigned char *ip = p + offset;
    ip[0] = 0x45 + options;
    ip[9] = 47;
    write16(ip + 2, 42 + options * 4);
    inet_pton(AF_INET, "192.0.2.2", ip + 12);
    inet_pton(AF_INET, "192.0.2.1", ip + 16);
    unsigned char *gre = ip + 20 + options * 4;
    write16(gre + 2, 0x6558);
    unsigned char *eth = gre + 4;
    eth[6] = 0x02;
    eth[11] = 0x42;
    write16(eth + 12, 0x8100);
    write16(eth + 14, 0xa3e8);
    write16(eth + 16, 0x0806); /* VLAN 1000 with PCP bits, ARP payload. */
    return offset + 42 + options * 4;
}

static void test_parser(void)
{
    unsigned char p[128];
    struct payload_packet parsed;
    for (unsigned options = 0; options <= 10; options++) {
        for (unsigned tags = 0; tags <= 2; tags++) {
            size_t len = packet(p, options, tags);
            g_assert_true(payload_parse(p, len, &parsed));
            g_assert_cmpuint(parsed.vlan, ==, 1000);
            g_assert_cmpuint(parsed.mac[5], ==, 0x42);
            for (size_t truncated = 0; truncated < len; truncated++)
                g_assert_false(payload_parse(p, truncated, &parsed));
        }
    }
    size_t len = packet(p, 0, 0);
    p[14] = 0x44;
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 20, 0x2000);
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 34, 0x2000);
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 36, 0x0800);
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    p[44] = 1;
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 52, 4095);
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 54, 0x8100);
    g_assert_false(payload_parse(p, len, &parsed));
    packet(p, 0, 0);
    write16(p + 20, 0x8000);
    g_assert_false(payload_parse(p, len, &parsed));
    /* Deterministic malformed input sweep runs under ASan/UBSan. */
    GRand *rng = g_rand_new_with_seed(42);
    for (unsigned i = 0; i < 20000; i++) {
        for (unsigned j = 0; j < sizeof(p); j++)
            p[j] = g_rand_int(rng);
        payload_parse(p, g_rand_int_range(rng, 0, sizeof(p)), &parsed);
    }
    g_rand_free(rng);
}

static bool load_text(const char *text)
{
    char *path = NULL;
    int fd = g_file_open_tmp("softgre-test-XXXXXX", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    g_assert_cmpint(write(fd, text, strlen(text)), ==, (ssize_t)strlen(text));
    close(fd);
    bool ok = softgred_config_load_conf(path);
    unlink(path);
    g_free(path);
    return ok;
}

static void test_config(void)
{
    softgred_config_release();
    g_assert_true(load_text("[global]\ninterface=eth0\ntunnel-prefix=sgre\n"
                            "bridge-map=1000@br-radius, 10@br-hotspot, 11@br-radius\n"
                            "bridge-force=true\n[service]\nport=65535\nmax-listen=128\n"
                            "[debug]\nenable=true\nprint-time=true\n"));
    struct softgred_config *cfg = softgred_config_ref();
    g_assert_cmpstr(cfg->bridge[0].ifname, ==, "br-radius"); /* Issue #5. */
    g_assert_cmpuint(cfg->bridge[0].vlan_id, ==, 1000);
    g_assert_cmpuint(cfg->bridge_slot, ==, 3);
    g_assert_cmpuint(cfg->srv_port, ==, 65535);
    g_assert_true(cfg->bridge_force);
    g_assert_true(cfg->dbg_time);
    const char *invalid[] = {
        "4095@br0",      "65537@br0", "-1@br0", "10oops@br0", "10@abcdefghijklmnop", "10@br;evil",
        "10@br0,10@br1", "10@br0,",   ""};
    for (unsigned i = 0; i < ARRAY_SIZE(invalid); i++) {
        char *text = g_strdup_printf("[global]\ninterface=eth0\nbridge-map=%s\n", invalid[i]);
        g_assert_false(load_text(text));
        g_free(text);
    }
    softgred_config_release();
    g_assert_true(load_text("[global]\ninterface=eth0\nbridge-map=10@br0\n"
                            "[debug]\nprint-time=true\n")); /* Missing optional service. */
    g_assert_true(cfg->dbg_time);
    g_assert_false(load_text("[global]\ninterface=eth0\nbridge-map=10@br0\n"
                             "[service]\nport=65536\n"));
    g_assert_false(load_text("[global]\ninterface=eth0\nbridge-map=10@br0\n"
                             "bridge-force=maybe\n"));
    g_assert_false(config_valid_ifname("../etc"));
    softgred_config_release();
}

static void test_provision(void)
{
    softgred_config_release();
    g_assert_true(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"));
    g_assert_cmpstr(softgred_config_ref()->log_file, ==, SOFTGRED_DEFAULT_LOGFILE);
    struct softgred_config *cfg = softgred_config_ref();
    inet_pton(AF_INET, "192.0.2.1", &cfg->local);
    provision_init();
    unsigned char p[128];
    struct payload_packet parsed;
    size_t len = packet(p, 0, 0);
    g_assert_true(payload_parse(p, len, &parsed));
    adds = allows = deletes = 0;
    provision_packet(&parsed);
    g_assert_cmpuint(adds, ==, 1);
    g_assert_cmpuint(allows, ==, 1);
    for (unsigned i = 0; i < 10000; i++)
        provision_packet(&parsed);
    g_assert_cmpuint(adds, ==, 1);
    g_assert_cmpuint(allows, ==, 1);
    GString *out = service_command("LMIP 192.0.2.2");
    g_assert_cmpstr(out->str, ==, "RESULT: OK\nBODY: 02:00:00:00:00:42\n");
    g_string_free(out, true);
    out = service_command("GTMC 02:00:00:00:00:42");
    g_assert_nonnull(strstr(out->str, "192.0.2.2@sgre0"));
    g_string_free(out, true);
    struct tunnel_context *tun = provision_get_tunnel_byip(&parsed.remote);
    for (unsigned i = 1; i < PROVISION_MAX_CLIENTS; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "02:01:00:00:%02x:%02x", i >> 8, i & 255);
        g_assert_true(provision_tunnel_allow_mac(tun, mac));
    }
    out = service_command("LMIP 192.0.2.2");
    g_assert_cmpuint(out->len, >, 18000);
    g_string_free(out, true);
    g_assert_false(provision_tunnel_allow_mac(tun, "02:ff:ff:ff:ff:ff"));
    struct in_addr remote;
    inet_pton(AF_INET, "192.0.2.3", &remote);
    struct tunnel_context *other = provision_add(&remote);
    g_assert_nonnull(other);
    g_assert_false(provision_tunnel_allow_mac(other, "02:00:00:00:00:42"));
    provision_delall();
    g_assert_cmpuint(g_hash_table_size(provision_table()), ==, 0);
    g_assert_null(provision_get_tunnel_by_mac("02:00:00:00:00:42"));
    fail_gre = true;
    g_assert_null(provision_add(&remote));
    fail_gre = false;
    fail_bridge = true;
    g_assert_null(provision_add(&remote));
    fail_bridge = false;
    g_assert_cmpuint(g_hash_table_size(provision_table()), ==, 0);
    other = provision_add(&remote);
    g_assert_nonnull(other);
    g_assert_cmpstr(other->ifgre, ==, "sgre0");
    fail_firewall = true;
    g_assert_false(provision_tunnel_allow_mac(other, "02:00:00:00:00:43"));
    g_assert_null(provision_get_tunnel_by_mac("02:00:00:00:00:43"));
    fail_firewall = false;
    fail_delete = true;
    provision_delall();
    g_assert_cmpuint(g_hash_table_size(provision_table()), ==, 1);
    g_assert_null(provision_add(&parsed.remote));
    fail_delete = false;
    provision_delall();
    g_assert_cmpuint(g_hash_table_size(provision_table()), ==, 0);
    provision_end();
    softgred_config_release();
}

static void test_commands(void)
{
    const char *bad[] = {"",
                         " ",
                         "VERSoops",
                         "VERS arg",
                         "LMIP",
                         "GTMC zz",
                         "LMIP 999.1.1.1",
                         "STUN a b",
                         "HELP a b c",
                         "GTMC 0:1:2:3:4:5"};
    for (unsigned i = 0; i < ARRAY_SIZE(bad); i++) {
        GString *out = service_command(bad[i]);
        g_assert_nonnull(strstr(out->str, "RESULT: INVALID"));
        g_string_free(out, true);
    }
    GString *out = service_command("STUN");
    g_assert_cmpstr(out->str, ==, "RESULT: NULL\n");
    g_string_free(out, true);
    out = service_command("LMIP 192.0.2.2");
    g_assert_cmpstr(out->str, ==, "RESULT: NOTFOUND\n");
    g_string_free(out, true);
}

static void test_pid(void)
{
    softgred_config_release();
    struct softgred_config *cfg = softgred_config_ref();
    char *path = NULL;
    int fd = g_file_open_tmp("softgre-pid-XXXXXX", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    close(fd);
    cfg->pid_file = g_strdup(path);
    g_assert_cmpint(softgred_config_create_pid_file(getpid()), ==, 0);
    pid_t child = fork();
    g_assert_cmpint(child, >=, 0);
    if (!child)
        _exit(softgred_config_create_pid_file(getpid()) == -1 ? 0 : 1);
    int status;
    g_assert_cmpint(waitpid(child, &status, 0), ==, child);
    g_assert_true(WIFEXITED(status));
    g_assert_cmpint(WEXITSTATUS(status), ==, 0);
    softgred_config_remove_pid_file();
    g_assert_cmpint(access(path, F_OK), ==, -1);
    /* Symlinks cannot redirect privileged PID writes. */
    g_assert_cmpint(symlink("/dev/null", path), ==, 0);
    g_assert_cmpint(softgred_config_create_pid_file(getpid()), ==, -1);
    unlink(path);
    g_free(path);
    softgred_config_release();
}

static void feed(const char *remote, const char *mac_text, uint16_t vlan)
{
    struct softgred_config *cfg = softgred_config_ref();
    struct payload_packet pk = {.local = cfg->local, .vlan = vlan};
    inet_pton(AF_INET, remote, &pk.remote);
    unsigned octet[6];
    sscanf(mac_text, "%x:%x:%x:%x:%x:%x", &octet[0], &octet[1], &octet[2], &octet[3], &octet[4],
           &octet[5]);
    for (unsigned i = 0; i < 6; i++)
        pk.mac[i] = octet[i];
    provision_packet(&pk);
}

/* A peer that fills a tunnel (a remotely triggerable capacity limit) must not arm the
 * shared backoff and stall provisioning for other peers. Regression for the global stall. */
static void test_capacity_backoff(void)
{
    softgred_config_release();
    g_assert_true(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"));
    struct softgred_config *cfg = softgred_config_ref();
    inet_pton(AF_INET, "192.0.2.1", &cfg->local);
    provision_init();
    fail_gre = fail_bridge = fail_firewall = false;
    feed("192.0.2.66", "02:00:00:00:00:01", 1000);
    struct tunnel_context *attacker = provision_get_tunnel_by_mac("02:00:00:00:00:01");
    g_assert_nonnull(attacker);
    for (unsigned i = 1; i < PROVISION_MAX_CLIENTS; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "02:aa:00:00:%02x:%02x", i >> 8, i & 255);
        g_assert_true(provision_tunnel_allow_mac(attacker, mac));
    }
    /* One more MAC on the full tunnel is a capacity rejection, not an operational failure. */
    adds = 0;
    feed("192.0.2.66", "02:bb:00:00:00:01", 1000);
    g_assert_cmpuint(adds, ==, 0);
    /* A fresh peer must provision immediately in the same instant, with no global gate. */
    feed("192.0.2.77", "02:00:00:00:00:02", 1000);
    g_assert_cmpuint(adds, ==, 1);
    g_assert_nonnull(provision_get_tunnel_by_mac("02:00:00:00:00:02"));
    provision_delall();
    provision_end();
    softgred_config_release();
}

/* Ingress authorization: an allowlist blocks spoofed/unknown remotes from provisioning,
 * which is the mitigation for MAC first-claim pinning and resource exhaustion. */
static void test_allow_remotes(void)
{
    softgred_config_release();
    g_assert_true(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"
                            "allow-remotes=192.0.2.0/24, 203.0.113.5\n"));
    struct softgred_config *cfg = softgred_config_ref();
    g_assert_cmpuint(cfg->allow_slot, ==, 2);
    inet_pton(AF_INET, "192.0.2.1", &cfg->local);
    provision_init();
    fail_gre = fail_bridge = fail_firewall = false;
    adds = 0;
    /* A spoofed remote outside the allowlist cannot claim the victim's MAC. */
    feed("198.51.100.9", "02:00:00:00:00:aa", 1000);
    g_assert_cmpuint(adds, ==, 0);
    g_assert_null(provision_get_tunnel_by_mac("02:00:00:00:00:aa"));
    /* The real AP, inside the allowlist, provisions and owns the MAC. */
    feed("192.0.2.2", "02:00:00:00:00:aa", 1000);
    g_assert_cmpuint(adds, ==, 1);
    g_assert_nonnull(provision_get_tunnel_by_mac("02:00:00:00:00:aa"));
    /* A single configured host (/32 default) is honored; its neighbor is not. */
    feed("203.0.113.5", "02:00:00:00:00:bb", 1000);
    g_assert_nonnull(provision_get_tunnel_by_mac("02:00:00:00:00:bb"));
    feed("203.0.113.6", "02:00:00:00:00:cc", 1000);
    g_assert_null(provision_get_tunnel_by_mac("02:00:00:00:00:cc"));
    provision_delall();
    provision_end();
    /* Malformed allowlists fail configuration loading; an absent key allows all remotes. */
    g_assert_false(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"
                             "allow-remotes=192.0.2.0/33\n"));
    g_assert_false(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"
                             "allow-remotes=not-an-ip\n"));
    g_assert_true(load_text("[global]\ninterface=eth0\nbridge-map=1000@br0\n"));
    g_assert_cmpuint(softgred_config_ref()->allow_slot, ==, 0);
    g_assert_true(config_remote_allowed(0x01020304));
    softgred_config_release();
}

#ifdef SOFTGRED_ENABLE_SERVICE
static void pump(unsigned cycles)
{
    for (unsigned i = 0; i < cycles; i++) {
        struct pollfd fds[1 + SERVICE_MAX_CLIENTS];
        unsigned n = service_pollfds(fds);
        g_assert_cmpint(poll(fds, n, 10), >=, 0);
        service_dispatch(fds, n);
    }
}

static int connect_api(unsigned port)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    g_assert_cmpint(fd, >=, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    g_assert_cmpint(connect(fd, (struct sockaddr *)&addr, sizeof(addr)), ==, 0);
    return fd;
}

static void test_api(void)
{
    softgred_config_release();
    struct softgred_config *cfg = softgred_config_ref();
    cfg->srv_bind_in = g_strdup("lo");
    cfg->srv_max_listen = 2;
    /* Get an ephemeral port, then release it for the server. */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    g_assert_cmpint(bind(fd, (struct sockaddr *)&addr, sizeof(addr)), ==, 0);
    socklen_t len = sizeof(addr);
    g_assert_cmpint(getsockname(fd, (struct sockaddr *)&addr, &len), ==, 0);
    cfg->srv_port = ntohs(addr.sin_port);
    close(fd);
    g_assert_cmpint(service_init(), ==, 0);
    int a = connect_api(cfg->srv_port), b = connect_api(cfg->srv_port);
    pump(2);
    g_assert_cmpint(send(a, "VE", 2, MSG_NOSIGNAL), ==, 2);
    pump(2);
    char buf[1024];
    g_assert_cmpint(recv(a, buf, sizeof(buf), MSG_DONTWAIT), ==, -1);
    g_assert_cmpint(errno, ==, EAGAIN);
    /* A slow client does not prevent a second client from finishing. */
    g_assert_cmpint(send(b, "STUN\r\n", 6, MSG_NOSIGNAL), ==, 6);
    pump(4);
    ssize_t got = recv(b, buf, sizeof(buf) - 1, 0);
    g_assert_cmpint(got, >, 0);
    buf[got] = '\0';
    g_assert_cmpstr(buf, ==, "RESULT: NULL\n");
    close(b);
    g_assert_cmpint(send(a, "RS\n", 3, MSG_NOSIGNAL), ==, 3);
    pump(4);
    got = recv(a, buf, sizeof(buf) - 1, 0);
    g_assert_cmpint(got, >, 0);
    buf[got] = '\0';
    g_assert_nonnull(strstr(buf, "RESULT: OK"));
    close(a);
    /* Half-close frames an unterminated command; closed peers never raise SIGPIPE. */
    a = connect_api(cfg->srv_port);
    pump(2);
    g_assert_cmpint(send(a, "VERS", 4, MSG_NOSIGNAL), ==, 4);
    shutdown(a, SHUT_WR);
    pump(4);
    got = recv(a, buf, sizeof(buf), 0);
    g_assert_cmpint(got, >, 0);
    close(a);
    a = connect_api(cfg->srv_port);
    pump(2);
    g_assert_cmpint(send(a, "VERS\n", 5, MSG_NOSIGNAL), ==, 5);
    close(a);
    pump(4);
    service_end();
    service_end();
    softgred_config_release();
}
#endif
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/packet/parser", test_parser);
    g_test_add_func("/config/types-and-bridges", test_config);
    g_test_add_func("/provision/state-and-rollback", test_provision);
    g_test_add_func("/provision/capacity-backoff", test_capacity_backoff);
    g_test_add_func("/provision/allow-remotes", test_allow_remotes);
    g_test_add_func("/service/commands", test_commands);
    g_test_add_func("/process/pid-lock-and-symlink", test_pid);
#ifdef SOFTGRED_ENABLE_SERVICE
    g_test_add_func("/service/socket-framing", test_api);
#endif
    return g_test_run();
}

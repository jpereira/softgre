/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesse General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesse General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesse General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */

#include "softgred_config.h"

#include "log.h"

#include <getopt.h>

static int pid_fd = -1;

struct softgred_config *softgred_config_ref(void)
{
    static struct softgred_config cfg = {.srv_port = 8888, .srv_max_listen = 10};
    return &cfg;
}

bool config_valid_ifname(const char *name)
{
    if (!name || !*name || strlen(name) >= IFNAMSIZ || !strcmp(name, ".") || !strcmp(name, ".."))
        return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!(ascii_isalnum(*p) || *p == '_' || *p == '-' || *p == '.'))
            return false;
    return true;
}

/* Minimal INI reader replacing GLib's GKeyFile: [group] headers, key=value lines, '#' and
 * ';' comments, whitespace trimmed. Later duplicate keys override earlier ones. */
struct ini_pair {
    char *group, *key, *value;
};

struct ini {
    struct ini_pair *pair;
    size_t count, cap;
};

static char config_error[256];

static void config_set_error(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void config_set_error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(config_error, sizeof(config_error), format, ap);
    va_end(ap);
}

static const char *ini_get(struct ini *ini, const char *group, const char *key)
{
    const char *value = NULL;
    for (size_t i = 0; i < ini->count; i++)
        if (!strcmp(ini->pair[i].group, group) && !strcmp(ini->pair[i].key, key))
            value = ini->pair[i].value; /* Keep scanning so the last definition wins. */
    return value;
}

static bool ini_has(struct ini *ini, const char *group, const char *key)
{
    return ini_get(ini, group, key) != NULL;
}

static void ini_free(struct ini *ini)
{
    if (!ini)
        return;
    for (size_t i = 0; i < ini->count; i++) {
        free(ini->pair[i].group);
        free(ini->pair[i].key);
        free(ini->pair[i].value);
    }
    free(ini->pair);
    free(ini);
}

/* Load the file; malformed lines are rejected rather than skipped so that a typo in a
 * security setting (for example a missing '=' in allow-remotes) can never silently fall back
 * to the permissive default. Returns NULL and sets config_error on failure. */
static struct ini *ini_load(const char *path)
{
    FILE *file = fopen(path, "re");
    if (!file) {
        config_set_error("%s", strerror(errno));
        return NULL;
    }
    struct ini *ini = xcalloc(1, sizeof(*ini));
    char *line = NULL, group[64] = "";
    size_t cap = 0;
    unsigned number = 0;
    bool ok = true;
    ssize_t length;
    while (ok && (length = getline(&line, &cap, file)) != -1) {
        number++;
        if (memchr(line, '\0', (size_t)length)) {
            config_set_error("line %u: embedded NUL byte", number);
            ok = false;
            break;
        }
        char *s = str_trim(line);
        if (!*s || *s == '#' || *s == ';')
            continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end || end[1] || end == s + 1 || end - s - 1 >= (long)sizeof(group)) {
                config_set_error("line %u: invalid group header", number);
                ok = false;
                break;
            }
            *end = '\0';
            str_lcpy(group, s + 1, sizeof(group));
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq || eq == s) {
            config_set_error("line %u: not a key=value pair", number);
            ok = false;
            break;
        }
        *eq = '\0';
        char *key = str_trim(s), *value = str_trim(eq + 1);
        if (!*key) {
            config_set_error("line %u: empty key", number);
            ok = false;
            break;
        }
        if (ini->count == ini->cap) {
            ini->cap = ini->cap ? ini->cap * 2 : 16;
            ini->pair = xrealloc(ini->pair, ini->cap * sizeof(*ini->pair));
        }
        ini->pair[ini->count].group = xstrdup(group);
        ini->pair[ini->count].key = xstrdup(key);
        ini->pair[ini->count].value = xstrdup(value);
        ini->count++;
    }
    if (ferror(file)) {
        config_set_error("cannot read configuration: %s", strerror(errno));
        ok = false;
    }
    free(line);
    fclose(file);
    if (!ok) {
        ini_free(ini);
        return NULL;
    }
    return ini;
}

static bool read_bool(struct ini *ini, const char *group, const char *key, bool *value)
{
    const char *text = ini_get(ini, group, key);
    if (!text)
        return true;
    bool valid =
        !strcmp(text, "true") || !strcmp(text, "false") || !strcmp(text, "1") || !strcmp(text, "0");
    if (valid)
        *value = !strcmp(text, "true") || !strcmp(text, "1");
    else
        config_set_error("[%s] %s must be true, false, 1 or 0", group, key);
    return valid;
}

static bool read_uint(struct ini *ini, const char *group, const char *key, unsigned *value,
                      unsigned min, unsigned max)
{
    const char *text = ini_get(ini, group, key);
    if (!text)
        return true;
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 10);
    bool valid = ascii_isdigit((unsigned char)*text) && !errno && end && !*end && parsed >= min &&
                 parsed <= max;
    if (valid)
        *value = parsed;
    else
        config_set_error("[%s] %s must be between %u and %u", group, key, min, max);
    return valid;
}

static char *read_string(struct ini *ini, const char *group, const char *key, const char *fallback)
{
    const char *text = ini_get(ini, group, key);
    return xstrdup(text ? text : fallback);
}

bool config_remote_allowed(uint32_t remote)
{
    struct softgred_config *cfg = softgred_config_ref();
    if (!cfg->allow_slot)
        return true;
    for (unsigned i = 0; i < cfg->allow_slot; i++)
        if ((remote & cfg->allow[i].mask) == cfg->allow[i].network)
            return true;
    return false;
}

/* Parse an optional allowlist of authorized remote IPv4 addresses or CIDR networks. */
static bool parse_allow(struct softgred_config *cfg, const char *list)
{
    char *copy = xstrdup(list), *cursor = copy, *field;
    bool ok = true;
    cfg->allow_slot = 0;
    while (ok && (field = str_split_next(&cursor, ','))) {
        char *entry = str_trim(field);
        if (!*entry) {
            ok = false;
            break;
        }
        if (cfg->allow_slot >= SOFTGRED_MAX_ALLOW) {
            ok = false;
            break;
        }
        char *slash = strchr(entry, '/');
        unsigned long bits = 32;
        if (slash) {
            char *end;
            *slash = '\0';
            errno = 0;
            bits = strtoul(slash + 1, &end, 10);
            if (!ascii_isdigit((unsigned char)slash[1]) || errno || *end || bits > 32) {
                ok = false;
                break;
            }
        }
        struct in_addr addr;
        if (inet_pton(AF_INET, entry, &addr) != 1) {
            ok = false;
            break;
        }
        uint32_t mask = bits ? htonl(~0u << (32 - bits)) : 0;
        struct allow_remote *slot = &cfg->allow[cfg->allow_slot++];
        slot->mask = ntohl(mask);
        slot->network = ntohl(addr.s_addr) & slot->mask;
    }
    free(copy);
    if (!ok)
        config_set_error("allow-remotes requires nonempty IPv4 addresses or CIDRs");
    return ok;
}

static bool parse_bridges(struct softgred_config *cfg, const char *map)
{
    char *copy = xstrdup(map), *cursor = copy, *field;
    bool ok = true;
    cfg->bridge_slot = 0;
    while ((field = str_split_next(&cursor, ','))) {
        char *vlan = str_trim(field), *at = strchr(vlan, '@'), *end;
        if (!at || cfg->bridge_slot >= SOFTGRED_MAX_ATTACH) {
            ok = false;
            break;
        }
        *at++ = '\0';
        str_trim(vlan);
        char *bridge = str_trim(at);
        errno = 0;
        unsigned long id = strtoul(vlan, &end, 10);
        if (!ascii_isdigit((unsigned char)*vlan) || errno || *end || id < 1 || id > 4094 ||
            !config_valid_ifname(bridge)) {
            ok = false;
            break;
        }
        for (unsigned i = 0; i < cfg->bridge_slot; i++)
            if (cfg->bridge[i].vlan_id == id)
                ok = false;
        if (!ok)
            break;
        struct tunnel_bridge *slot = &cfg->bridge[cfg->bridge_slot++];
        str_lcpy(slot->ifname, bridge, sizeof(slot->ifname));
        slot->vlan_id = id;
    }
    free(copy);
    return ok && cfg->bridge_slot > 0;
}

bool softgred_config_load_conf(const char *path)
{
    struct softgred_config *cfg = softgred_config_ref();
    config_error[0] = '\0';
    struct ini *file = ini_load(path);
    bool ok = file != NULL;
    if (!ok)
        goto done;
    free(cfg->ifname);
    cfg->ifname = read_string(file, "global", "interface", NULL);
    free(cfg->tunnel_prefix);
    cfg->tunnel_prefix = read_string(file, "global", "tunnel-prefix", "sgre");
    char *map = read_string(file, "global", "bridge-map", NULL);
    ok = config_valid_ifname(cfg->ifname) && config_valid_ifname(cfg->tunnel_prefix) &&
         strlen(cfg->tunnel_prefix) <= 6 && map && parse_bridges(cfg, map);
    free(map);
    if (!ok)
        goto done;
    cfg->allow_slot = 0;
    if (ini_has(file, "global", "allow-remotes")) {
        char *list = read_string(file, "global", "allow-remotes", "");
        ok = list && parse_allow(cfg, list);
        free(list);
        if (!ok)
            goto done;
    }
    if (!cfg->pid_file)
        cfg->pid_file = read_string(file, "global", "pid-file", SOFTGRED_DEFAULT_PIDFILE);
    if (!cfg->log_file)
        cfg->log_file = read_string(file, "global", "log-file", SOFTGRED_DEFAULT_LOGFILE);
    if (!cfg->log_file || !path_is_absolute(cfg->log_file) || strpbrk(cfg->log_file, "\r\n")) {
        ok = false;
        goto done;
    }
    free(cfg->srv_bind_in);
    cfg->srv_bind_in = read_string(file, "service", "bind-in", "lo");
    ok = cfg->srv_bind_in &&
         (config_valid_ifname(cfg->srv_bind_in) || !strcmp(cfg->srv_bind_in, "any"));
    bool timestamp = false;
    ok = ok && read_bool(file, "global", "bridge-force", &cfg->bridge_force) &&
         read_bool(file, "debug", "enable", &cfg->dbg_enable) &&
         read_bool(file, "debug", "print-time", &timestamp) &&
         read_bool(file, "debug", "time", &timestamp) &&
         read_uint(file, "service", "port", &cfg->srv_port, 1, 65535) &&
         read_uint(file, "service", "max-listen", &cfg->srv_max_listen, 1, 128);
    cfg->dbg_time = cfg->dbg_time || timestamp;
    if (cfg->dbg_enable && !cfg->dbg_mode)
        cfg->dbg_mode = 1;
    cfg->started_time = time(NULL);
    uname(&cfg->uts);
done:
    if (!ok)
        fprintf(stderr, "Configuration %s: %s\n", path,
                config_error[0] ? config_error
                                : "invalid interface, prefix, bridge map or log path");
    ini_free(file);
    return ok;
}

int softgred_config_load_cli(int argc, char **argv)
{
    struct softgred_config *cfg = softgred_config_ref();
    static const struct option opts[] = {{"config-file", required_argument, NULL, 'c'},
                                         {"foreground", no_argument, NULL, 'f'},
                                         {"pid-file", required_argument, NULL, 'p'},
                                         {"log-file", required_argument, NULL, 'l'},
                                         {"debug", no_argument, NULL, 'd'},
                                         {"debug-time", no_argument, NULL, 't'},
                                         {"help", no_argument, NULL, 'h'},
                                         {"version", no_argument, NULL, 'v'},
                                         {NULL, 0, NULL, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, "c:fp:l:dthv", opts, NULL)) != -1) {
        switch (opt) {
        case 'c':
            free(cfg->config_file);
            cfg->config_file = xstrdup(optarg);
            break;
        case 'p':
            free(cfg->pid_file);
            cfg->pid_file = xstrdup(optarg);
            break;
        case 'l':
            free(cfg->log_file);
            cfg->log_file = xstrdup(optarg);
            break;
        case 'f':
            cfg->is_foreground = true;
            break;
        case 'd':
            cfg->dbg_mode = MIN(cfg->dbg_mode + 1, 4);
            break;
        case 't':
            cfg->dbg_time = true;
            break;
        case 'v':
            printf("%s\n", PACKAGE_STRING);
            return 0;
        case 'h':
            printf("Usage: softgred [-f] [-c FILE] [-p FILE] [-l FILE] [-d...] [-t] [-h] [-v]\n"
                   "  -c, --config-file  Configuration (default: %s)\n"
                   "  -f, --foreground   Run in foreground; also logs to stderr\n"
                   "  -p, --pid-file     Locked PID file (default: %s)\n"
                   "  -l, --log-file     Append log file (default: %s)\n"
                   "  -d, --debug        Increase debug level (up to four)\n"
                   "  -t, --debug-time   Timestamp log messages\n"
                   "  -h, --help         Show help\n"
                   "  -v, --version      Show version\n",
                   SOFTGRED_DEFAULT_CONFFILE, SOFTGRED_DEFAULT_PIDFILE, SOFTGRED_DEFAULT_LOGFILE);
            return 0;
        default:
            return -1;
        }
    }
    return optind == argc ? 1 : -1;
}

void softgred_config_release(void)
{
    struct softgred_config *cfg = softgred_config_ref();
    free(cfg->config_file);
    free(cfg->ifname);
    free(cfg->tunnel_prefix);
    free(cfg->pid_file);
    free(cfg->srv_bind_in);
    free(cfg->log_file);
    free(cfg->dbg_file);
    memset(cfg, 0, sizeof(*cfg));
    cfg->srv_port = 8888;
    cfg->srv_max_listen = 10;
}

int softgred_config_create_pid_file(int pid)
{
    const char *path = softgred_config_ref()->pid_file;
    pid_fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (pid_fd < 0)
        return -1;
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
    struct stat st;
    if (fstat(pid_fd, &st))
        goto fail;
    if (!S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_uid != geteuid() ||
        (st.st_mode & (S_IWGRP | S_IWOTH))) {
        errno = EPERM;
        goto fail;
    }
    if (fcntl(pid_fd, F_SETLK, &lock))
        goto fail;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", pid);
    if (ftruncate(pid_fd, 0) || write(pid_fd, buf, len) != len)
        goto fail;
    return 0;
fail:
    close(pid_fd);
    pid_fd = -1;
    return -1;
}

void softgred_config_remove_pid_file(void)
{
    if (pid_fd >= 0) {
        unlink(softgred_config_ref()->pid_file);
        close(pid_fd);
        pid_fd = -1;
    }
}

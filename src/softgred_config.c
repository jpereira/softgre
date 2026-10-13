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
        if (!(g_ascii_isalnum(*p) || *p == '_' || *p == '-' || *p == '.'))
            return false;
    return true;
}

static bool read_bool(GKeyFile *file, const char *group, const char *key, bool *value,
                      GError **error)
{
    if (!g_key_file_has_key(file, group, key, NULL))
        return true;
    char *text = g_key_file_get_string(file, group, key, error);
    if (!text)
        return false;
    g_strstrip(text);
    bool valid =
        !strcmp(text, "true") || !strcmp(text, "false") || !strcmp(text, "1") || !strcmp(text, "0");
    if (valid)
        *value = !strcmp(text, "true") || !strcmp(text, "1");
    else
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                    "[%s] %s must be true, false, 1 or 0", group, key);
    g_free(text);
    return valid;
}

static bool read_uint(GKeyFile *file, const char *group, const char *key, unsigned *value,
                      unsigned min, unsigned max, GError **error)
{
    if (!g_key_file_has_key(file, group, key, NULL))
        return true;
    char *text = g_key_file_get_string(file, group, key, error), *end = NULL;
    if (!text)
        return false;
    g_strstrip(text);
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 10);
    bool valid = g_ascii_isdigit(*text) && !errno && end && !*end && parsed >= min && parsed <= max;
    if (valid)
        *value = parsed;
    else
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                    "[%s] %s must be between %u and %u", group, key, min, max);
    g_free(text);
    return valid;
}

static char *read_string(GKeyFile *file, const char *group, const char *key, const char *fallback,
                         GError **error)
{
    if (!g_key_file_has_key(file, group, key, NULL))
        return g_strdup(fallback);
    char *text = g_key_file_get_string(file, group, key, error);
    return text ? g_strstrip(text) : NULL;
}

static bool parse_bridges(struct softgred_config *cfg, const char *map)
{
    char **entries = g_strsplit(map, ",", -1);
    bool ok = true;
    cfg->bridge_slot = 0;
    for (char **p = entries; *p; p++) {
        char *vlan = g_strstrip(*p), *at = strchr(vlan, '@'), *end;
        if (!at || cfg->bridge_slot >= SOFTGRED_MAX_ATTACH) {
            ok = false;
            break;
        }
        *at++ = '\0';
        g_strstrip(vlan);
        char *bridge = g_strstrip(at);
        errno = 0;
        unsigned long id = strtoul(vlan, &end, 10);
        if (!g_ascii_isdigit(*vlan) || errno || *end || id < 1 || id > 4094 ||
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
        g_strlcpy(slot->ifname, bridge, sizeof(slot->ifname));
        slot->vlan_id = id;
    }
    g_strfreev(entries);
    return ok && cfg->bridge_slot > 0;
}

bool softgred_config_load_conf(const char *path)
{
    struct softgred_config *cfg = softgred_config_ref();
    GKeyFile *file = g_key_file_new();
    GError *error = NULL;
    bool ok = g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &error);
    if (!ok)
        goto done;
    g_free(cfg->ifname);
    cfg->ifname = read_string(file, "global", "interface", NULL, &error);
    g_free(cfg->tunnel_prefix);
    cfg->tunnel_prefix = read_string(file, "global", "tunnel-prefix", "sgre", &error);
    char *map = read_string(file, "global", "bridge-map", NULL, &error);
    ok = config_valid_ifname(cfg->ifname) && config_valid_ifname(cfg->tunnel_prefix) &&
         strlen(cfg->tunnel_prefix) <= 6 && map && parse_bridges(cfg, map);
    g_free(map);
    if (!ok)
        goto done;
    if (!cfg->pid_file)
        cfg->pid_file = read_string(file, "global", "pid-file", SOFTGRED_DEFAULT_PIDFILE, &error);
    if (!cfg->log_file)
        cfg->log_file = read_string(file, "global", "log-file", SOFTGRED_DEFAULT_LOGFILE, &error);
    if (!cfg->log_file || !g_path_is_absolute(cfg->log_file) || strpbrk(cfg->log_file, "\r\n")) {
        ok = false;
        goto done;
    }
    g_free(cfg->srv_bind_in);
    cfg->srv_bind_in = read_string(file, "service", "bind-in", "lo", &error);
    ok = cfg->srv_bind_in &&
         (config_valid_ifname(cfg->srv_bind_in) || !strcmp(cfg->srv_bind_in, "any"));
    bool timestamp = false;
    ok = ok && read_bool(file, "global", "bridge-force", &cfg->bridge_force, &error) &&
         read_bool(file, "debug", "enable", &cfg->dbg_enable, &error) &&
         read_bool(file, "debug", "print-time", &timestamp, &error) &&
         read_bool(file, "debug", "time", &timestamp, &error) &&
         read_uint(file, "service", "port", &cfg->srv_port, 1, 65535, &error) &&
         read_uint(file, "service", "max-listen", &cfg->srv_max_listen, 1, 128, &error);
    cfg->dbg_time = cfg->dbg_time || timestamp;
    if (cfg->dbg_enable && !cfg->dbg_mode)
        cfg->dbg_mode = 1;
    cfg->started_time = time(NULL);
    uname(&cfg->uts);
    ok = ok && !error;
done:
    if (!ok)
        fprintf(stderr, "Configuration %s: %s\n", path,
                error ? error->message : "invalid interface, prefix, bridge map or log path");
    g_clear_error(&error);
    g_key_file_free(file);
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
            g_free(cfg->config_file);
            cfg->config_file = g_strdup(optarg);
            break;
        case 'p':
            g_free(cfg->pid_file);
            cfg->pid_file = g_strdup(optarg);
            break;
        case 'l':
            g_free(cfg->log_file);
            cfg->log_file = g_strdup(optarg);
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
    g_free(cfg->config_file);
    g_free(cfg->ifname);
    g_free(cfg->tunnel_prefix);
    g_free(cfg->pid_file);
    g_free(cfg->srv_bind_in);
    g_free(cfg->log_file);
    g_free(cfg->dbg_file);
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
    if (fstat(pid_fd, &st) || !S_ISREG(st.st_mode) || fcntl(pid_fd, F_SETLK, &lock))
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

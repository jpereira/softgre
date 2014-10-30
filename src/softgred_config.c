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

#define _GNU_SOURCE
#include "general.h"
#include "softgred_config.h"
#include "helper.h"
#include "log.h"

#include "version.h" /* Generated automatic */

// version.h
#ifndef CURRENT_COMMIT
#   warning "oops! the file 'version.h' wasn't created!"
#   define CURRENT_COMMIT "not defined"
#endif
#ifndef CURRENT_BRANCH
#   warning "oops! the file 'version.h' wasn't created!"
#   define CURRENT_BRANCH "not defined"
#endif

struct softgred_config *
softgred_config_ref()
{
    /* below default settings */
    static struct softgred_config ref = {
        // global
        .config_file   = NULL,
        .is_foreground = false,
        .ifname        = NULL,
        .tunnel_prefix = NULL,
        .pid_file      = NULL,
        .bridge_force  = false,
        .bridge = {
            { '\0', 0 },
         },
        .bridge_slot = 0,

        // service
        .srv_bind_in    = NULL,
        .srv_port       = 8888,
        .srv_max_listen = 10,

        // debug
        .dbg_enable    = false,
        .dbg_file      = NULL,
        .dbg_mode      = 0,
        .dbg_time      = false,

        // priv
        .table = NULL,

        .debug_env = {
            .payload   = false,
            .cmd       = false,
            .provision = false,
            .service   = false,
        },

        .priv = {
            .ifname_ip    = { 0, },
            .ifname_saddr = { 0, }
        }
    };

    return &ref;
}

__attribute__ ((destructor)) void
softgred_config_release()
{
    struct softgred_config *cfg = softgred_config_ref();
    char *ptrs[] = { /* pointers to be released */
        cfg->config_file,
        cfg->ifname,
        cfg->tunnel_prefix,
        cfg->log_file,
        cfg->pid_file,
        cfg->srv_bind_in,
        cfg->dbg_file
    };
    int n = 0;

    for (n=0; n < ARRAY_SIZE(ptrs); n++)
    {
        if (ptrs[n] != NULL)
        {
            free(ptrs[n]);
            ptrs[n] = NULL;
        }
    }
}

int
softgred_config_init()
{
    struct softgred_config *cfg = softgred_config_ref();
    int error;

    cfg->started_time = time(NULL);
    uname(&cfg->uts);

    if ((error = hash_create(0, &cfg->table, NULL,  NULL)) != HASH_SUCCESS)
    {
        fprintf(stderr, "cannot create hash table (%s)\n", hash_error_string(error));
        return false;
    }

    return true;
}

int
softgred_config_end()
{
    struct softgred_config *cfg = softgred_config_ref();

    D_INFO("Releasing allocated memory in config system\n");

    /* Free the table */
    hash_destroy(cfg->table);

    return true;
}

int
config_set_interface(struct softgred_config *cfg,
                     const char *ifname)
{
    struct sockaddr_in *sin = &cfg->priv.ifname_saddr;
    struct ifaddrs *ifap, *ifa;
    size_t ifacen = strnlen(ifname, SOFTGRED_MAX_IFACE);

    assert (ifname != NULL);
    assert (cfg != NULL);
    assert (sin != NULL);

    if (cfg->ifname)
    {
        D_CRIT("The parameter 'interface' is already set with '%s'. releasing!\n", cfg->ifname);
        free(cfg->ifname);
        cfg->ifname = NULL;
    }

    getifaddrs (&ifap);

    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (strncmp(ifname, ifa->ifa_name, ifacen))
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            char *addr = inet_ntoa(sa->sin_addr);
            
            cfg->ifname = strdup(ifname);
            strncpy(cfg->priv.ifname_ip, addr, SOFTGRED_MAX_IFACE);
            memcpy(sin, sa, sizeof(struct sockaddr_in));

            break;
        }
    }

    freeifaddrs(ifap);

    if (!cfg->ifname)
    {
        fprintf(stderr, "** impossible to listening in '%s' network interface, exiting...\n", ifname);
        return 0;
    }

    return true;
}

int
config_set_tunnel_prefix(struct softgred_config *cfg,
                         const char *tunnel_prefix)
{
    D_DEBUG3("argument is '%s'\n", tunnel_prefix);

    if (!cfg->tunnel_prefix)
        cfg->tunnel_prefix = strdup (tunnel_prefix);

    return true;
}

int
config_set_bridge_map(struct softgred_config *cfg,
                      const char *br_ifaces)
{
    gchar **strings;
    gchar **ptr;
    bool status = false;

    D_DEBUG3("argument is '%s'\n", br_ifaces);

    strings = g_strsplit(br_ifaces, ",", 10);
    for (ptr = strings; *ptr; ptr++)
    {
        char *arg = *ptr;
        char *tmp = (char *)g_strstrip(arg);
        const char *str_vlan_id = strtok((char *)arg, "@");
        const char *br_name = strtok(NULL, "@");
        uint8_t pos = cfg->bridge_slot;

        // have args?
        if (!str_vlan_id || !br_name)
        {
            fprintf(stderr, "** error! wrong argument! expected is vlan_id@bridge-to-attach, eg.: -a 10@br-vlan123\n");
            break;
        }

        // vlan validate
        uint16_t vlan_id = strtol(str_vlan_id, NULL, 10);
        if (vlan_id < 1 || vlan_id > 4096)
        {
            fprintf(stderr, "** error! The argument '%s' is a wrong vlan id, exiting...\n", str_vlan_id);
            break;
        }

        // bridge validate, expected: "<name><number>" <= SOFTGRED_MAX_IFACE
        size_t br_len = strnlen(br_name, 64);
        if (br_len < 3)
        {
            fprintf(stderr, "** error! The argument '%s' (%ld) is a wrong bridge name. (len >= 3 && len <= %d)\n", 
                                                            br_name, br_len,  SOFTGRED_TUN_PREFIX_MAX);
            break;
        }

        if (if_nametoindex(br_name) < 1) // TODO: Change for validate if is a real bridge-interface.
        {
            fprintf(stderr, "** error! The bridge '%s' don't exist in your system! try to create, eg.: brctl addbr %s\n",
                                br_name, br_name);
            break;
        }

        if (pos >= SOFTGRED_MAX_ATTACH)
        {
            fprintf(stderr, "** error! The maximum number of slots was reached.\n");
            break;
        }

        // adding arguments
        D_DEBUG3("Loading the argument '%s' { .vlan_id='%d', .br_iface='%s' }\n", tmp, vlan_id, br_name);
        strcpy(cfg->bridge[pos].ifname, br_name);
        cfg->bridge[pos].vlan_id = vlan_id;
        cfg->bridge_slot += 1;
        status = true;
    }

    g_strfreev (strings);

    return status;
}

bool
config_set_dbg_mode(struct softgred_config *cfg,
                      const char *val)
{
    D_DEBUG3("argument is '%s'\n", val);

    return true;
}

int
softgred_config_load_conf(const char *config_file)
{
    struct softgred_config *cfg = softgred_config_ref();
    struct softgred_config_map cfg_maps[] = {
        { "global",  "interface",     true,  T_CALLBACK, (void **)&config_set_interface     },
        { "global",  "tunnel-prefix", true,  T_CALLBACK, (void **)&config_set_tunnel_prefix },
        { "global",  "bridge-map",    true,  T_CALLBACK, (void **)&config_set_bridge_map    },
        { "global",  "bridge-force",  false, T_BOOL,     (void **)&cfg->bridge_force        },
        { "global",  "pid-file",      true,  T_STRING,   (void **)&cfg->pid_file            },
        { "global",  "log-file",      true,  T_STRING,   (void **)&cfg->log_file            },

        { "service", "bind-in",       false, T_STRING,   (void **)&cfg->srv_bind_in         },
        { "service", "port",          false, T_INTEGER,  (void **)&cfg->srv_port            },
        { "service", "max-listen",    false, T_INTEGER,  (void **)&cfg->srv_max_listen      },

        { "debug",   "enable",        false, T_BOOL,     (void **)&cfg->dbg_enable          },
        { "debug",   "mode",          false, T_CALLBACK, (void **)&config_set_dbg_mode      },
        { "debug",   "file",          false, T_STRING,   (void **)&cfg->dbg_file            },
        { "debug",   "time",          false, T_STRING,   (void **)&cfg->dbg_time            },
    };
    GKeyFile *keyfile;
    GError *error = NULL;
    int i = 0;
    bool is_ok = true;

    D_INFO("Loading the '%s' config file.\n", config_file);

    if (access(config_file, R_OK))
    {
        fprintf(stderr, "** Problems with config file %s! errno=%d(%s)\n", config_file,
                                    errno, strerror(errno));
        return false;
    }

    keyfile = g_key_file_new ();

    if (!g_key_file_load_from_file (keyfile, config_file, G_KEY_FILE_NONE, NULL))
    {
        fprintf(stderr, "The config file %s is invalid, exiting...\n", config_file);
        g_key_file_free(keyfile);
        return false;
    }

    for (i=0; is_ok && i < ARRAY_SIZE(cfg_maps); i++)
    {
        struct softgred_config_map *entry = &cfg_maps[i];
        gchar *val = NULL;

        D_DEBUG3("Checking the key [%s]::%s\n", entry->group, entry->key);
        val = g_key_file_get_value (keyfile, entry->group, entry->key, NULL);
        if (!val)
        {
            if (entry->is_necessary)
            {
                D_INFO("Impossible to continue without the key '%s' in [%s], exiting!\n", 
                                    entry->key, entry->group);
                is_ok = false;
            }

            break;
        }

        val = g_strstrip(val);

        D_DEBUG3("Setting the key [%s]::%s with '%s'\n", entry->group, entry->key, val);

        switch(entry->type)
        {
            case T_STRING: {
                    char **ptr = ((char **)entry->ptr);

                    if (*ptr)
                    {
                        D_INFO("the pointer of key [%s]::%s is already set with '%s', ignoring '%s'.\n",
                                                entry->group, entry->key, *ptr, val
                        );
                    }
                    else
                    {
                        *ptr = strdup (val);
                    }
                }
                break;

            case T_INTEGER:
            case T_BOOL: {
                   int32_t num = strtol(val, NULL, 10);

                   *((bool *)entry->ptr) = (entry->type == T_BOOL) ? (num == true) : num;
                }
                break;

            case T_CALLBACK: {
                    int (*cb)(struct softgred_config *, const char *) = (void *)entry->ptr;

                    if (!cb(cfg, val))
                    {
                        D_CRIT("Problems with parameters for key '%s' in [%s], exiting!\n", 
                                                                            entry->key, entry->group);
                        is_ok = false;
                    }
                }
                break;

            default:
                assert(entry->type);
        }
    }

    g_key_file_free (keyfile);

    return is_ok;
}

int
softgred_config_load_cli(int argc, 
                         char *argv[])
{
    struct option long_opts[] = {
        { "config-file",   required_argument, NULL, 'c'},
        { "foreground",    no_argument,       NULL, 'f'},
        { "pid-file",      required_argument, NULL, 'p'},
        { "log-file",      required_argument, NULL, 'l'},
        { "debug",         optional_argument, NULL, 'd'},
        { "debug-time",    no_argument,       NULL, 't'},
        { "help",          no_argument,       NULL, 'h'},
        { "version",       no_argument,       NULL, 'v'},
        { NULL,            0,                 NULL,  0 }
    };
    struct softgred_config *cfg = softgred_config_ref();
    int opt;

    while ((opt = getopt_long(argc, argv, "c:fp:l:dthv", long_opts, NULL)) != EOF)
    {
        switch (opt)
        {
            case 'c': /* --config-file */
                if (!optarg)
                {
                    fprintf(stderr, "option '-c', is necessary to set the path of config file\n");
                    return false;
                }
                cfg->config_file = strdup(optarg);
                break;

            case 'f': /* --foreground */
                cfg->is_foreground = true;
                break;
            case 'p': /* --pid-file */
                if (optarg)
                    cfg->pid_file = strdup(optarg);
                break;
            case 'l': /* --log-file */
                if (optarg)
                    cfg->log_file = strdup(optarg);
                break;
            case 'd': /* --debug */
                cfg->dbg_mode += 1;
                break;
            case 't': /* --debug-time */
                cfg->dbg_time = true;
                break;
            case 'h': /* --help */
                softgred_print_usage(argv);
                return false;
            case 'v': /* --version */
                softgred_print_version();
                return false;
            default:
                softgred_print_usage(argv);
                return false;
        }
    }

    /* Check debug level */
    if (cfg->dbg_mode > 0)
    {
        if (cfg->dbg_mode > DEBUG_MAX_LEVEL)
        {
            fprintf(stderr, "*** Ops!! the maximum of debug level is %d (-ddd).\n", DEBUG_MAX_LEVEL);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "*** Entering in debug mode with level %d! ***\n", cfg->dbg_mode);
    }

    D_INFO("** SoftGREd %s (Build %s - %s) - Daemon Started **\n", PACKAGE_VERSION, __TIME__, __DATE__);

    return true;
}

void
softgred_print_version()
{
    printf("%s By Jorge Pereira <jpereiran@gmail.com>\n", PACKAGE_STRING);
    printf("Daemon Built:    %s - %s\n", __TIME__, __DATE__);
    printf("Project Website: %s\n", PACKAGE_URL);
    printf("Bug Report:      %s\n", PACKAGE_BUGREPORT);
    printf("GIT Branch:      %s\n", CURRENT_BRANCH);
    printf("GIT Commit:      %s\n", CURRENT_COMMIT);
}

void
softgred_print_usage(char *argv[])
{
    const char *arg0 = basename (argv[0]);
    const char *default_conf = SOFTGRED_DEFAULT_CONFFILE;
    const char *default_pid = SOFTGRED_PIDDIR"/"PACKAGE".pid";

    printf("Usage: %s [-c %s] [-f] [-d] [-dd] [-ddd] [-thv] [ -p pid-file ]\n" \
           "\n" \
           " OPTIONS\n" \
           "\n" \
           "   -c, --config-file    Path for softgred.conf, (default: %s)\n"  \
           "   -f, --foreground     Run in the foreground\n"  \
           "   -d, --debug          Enable debug mode. e.g: -dd (more debug), -ddd (crazy debug level)\n"      \
           "   -t, --debug-time     Show the current time in debug/messages.\n"      \
           "   -p, --pid-file       Path of pid-file, default in %s\n" \
           "   -l, --log-file       Path of log-file, default in %s\n" \
           "   -h, --help           Display this help text\n" \
           "   -v, --version        Display the %s version\n" \
           "\n", arg0, default_conf, default_conf, default_pid, arg0, SOFTGRED_DEFAULT_LOGFILE);
}

int
softgred_config_create_pid_file(int pid)
{
    struct softgred_config *cfg = softgred_config_ref();
    int fd;
    const char *prog_name = program_invocation_short_name;
    char buf[100];
    int flags = 1;
    struct flock fl = {
            .l_type = F_WRLCK,
            .l_whence = 0,
            .l_start = 0,
            .l_len = 0
    };

    D_DEBUG3("Saving pidfile in %s\n", cfg->pid_file);

    fd = open(cfg->pid_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        D_CRIT("Could not open PID file %s, errno=%d(%s)\n", cfg->pid_file, 
                    errno, strerror(errno));
        return -1;
    }

    if (flags & 1)
    {
        /* Instead of the following steps, we could (on Linux) have opened the
           file with O_CLOEXEC flag. However, not all systems support open()
           O_CLOEXEC (which was standardized only in SUSv4), so instead we use
           fcntl() to set the close-on-exec flag after opening the file */

        flags = fcntl(fd, F_GETFD);                     /* Fetch flags */
        if (flags == -1)
        {
            D_CRIT("Could get flags for PID file %s, errno=%d(%s)\n", cfg->pid_file, 
                    errno, strerror(errno));
            return -1;
        }

        flags |= FD_CLOEXEC;                            /* Turn on FD_CLOEXEC */
        if (fcntl(fd, F_SETFD, flags) == -1)            /* Update flags */
        {
            D_CRIT("Could set flags for PID file %s, errno=%d(%s)\n", cfg->pid_file, 
                    errno, strerror(errno));
            return -1;
        }
    }

    if (fcntl(fd, F_SETLK, &fl) == -1)
    {
        if (errno  == EAGAIN || errno == EACCES)
            D_CRIT("PID file '%s' is locked; probably ""'%s' is already running", cfg->pid_file, 
                                    prog_name);
        else
            D_CRIT("Unable to lock PID file '%s'", cfg->pid_file);

        return -1;
    }

    if (ftruncate(fd, 0) == -1)
    {
        D_CRIT("Could not truncate PID file '%s'", cfg->pid_file);
        return -1;
    }

    size_t len = snprintf(buf, sizeof(buf), "%ld\n", pid);
    if (write(fd, buf, strlen(buf)) != len)
    {
        D_CRIT("Could not write file '%s'", cfg->pid_file);
        return -1;
    }

    return 0;
}


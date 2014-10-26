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
softgred_config_get_ref()
{
    /* below default settings */
    static struct softgred_config ref = {
        .is_foreground = false,
        .ifname        = NULL,
        .tunnel_prefix = SOFTGRED_TUN_PREFIX,
        .debug_mode    = 0,
        .debug_xmode   = false,
        .print_time    = false,
        .pid_file      = { '\0', },
        .debug_env = {
            .payload   = false,
            .cmd       = false,
            .provision = false,
            .service   = false,
        },
        .bridge = {
            { NULL, 0 },
        },
        .bridge_slot = 0,
        .priv = {
            .ifname_ip    = { 0, },
            .ifname_saddr = { 0, }
        }
    };

    return &ref;
}

int
softgred_config_init()
{
    struct softgred_config *cfg = softgred_config_get_ref();
    int error;

    if ((error = hash_create(0, &cfg->table, NULL,  NULL)) != HASH_SUCCESS)
    {
        fprintf(stderr, "cannot create hash table (%s)\n", hash_error_string(error));
        return error;
    }

    /* check and enable options */
    softgred_config_load_envs();

    return 0;
}

int
softgred_config_end()
{
    struct softgred_config *cfg = softgred_config_get_ref();

    /* Free the table */
    hash_destroy(cfg->table);

    return 0;
}

void
softgred_config_load_envs()
{
    struct softgred_config *cfg = softgred_config_get_ref();
    struct softgred_config_debug_env debug_envs[] = {
        { "SOFTGRED_DEBUG_PAYLOAD",   &(softgred_config_get_ref())->debug_env.payload    },
        { "SOFTGRED_DEBUG_CMD",       &(softgred_config_get_ref())->debug_env.cmd        },
        { "SOFTGRED_DEBUG_PROVISION", &(softgred_config_get_ref())->debug_env.provision  },
        { "SOFTGRED_DEBUG_SERVICE",   &(softgred_config_get_ref())->debug_env.service    },
    };
    size_t i = 0;

    /* check and enable options */
    for (i=0; i < ARRAY_SIZE(debug_envs); i++)
    {
        struct softgred_config_debug_env *env = &debug_envs[i];

        if (cfg->debug_xmode)
            *env->flag = true;
        else
            *env->flag = getenv(env->var);

        D_DEBUG3("Checking variable %s (%s)\n", env->var, print_bool(*env->flag));
    }
}

int
softgred_config_load_cli(int argc, 
                         char *argv[])
{
    struct option long_opts[] = {
        { "foreground",    no_argument,       NULL, 'f'},
        { "iface",         required_argument, NULL, 'i'},
        { "tunnel-prefix", optional_argument, NULL, 'P'},
        { "attach",        required_argument, NULL, 'a'},
        { "debug",         optional_argument, NULL, 'd'},
        { "xdebug",        optional_argument, NULL, 'x'},
        { "print-time",    no_argument,       NULL, 't'},
        { "pid-file",      required_argument, NULL, 'p'},
        { "help",          no_argument,       NULL, 'h'},
        { "version",       no_argument,       NULL, 'v'},
        { NULL,            0,                 NULL,  0 }
    };
    struct softgred_config *cfg = softgred_config_get_ref();
    int opt;

    /* parsing arguments ... */
    while ((opt = getopt_long(argc, argv, "fhvti:a:P:p:dx", long_opts, NULL)) != EOF)
    {
        switch (opt)
        {
            case 'f': /* --foreground */
                cfg->is_foreground = true;
                break;
            case 'P': /* --tunnel-prefix */
                cfg->tunnel_prefix = optarg;
                break;
            case 'i': /* --iface */
                if (!softgred_config_load_iface(optarg, cfg))
                    return 0;
                break;
            case 'a': /* --attach */
                if (!softgred_config_load_attach(optarg, cfg))
                    return 0;
                break;
            case 'h': /* --help */
                softgred_print_usage(argv);
                exit(EXIT_SUCCESS);
            case 'd': /* --debug */
                cfg->debug_mode += 1;
                break;
            case 'x': /* --xdebug */
                D_DEBUG0("Enabling all debug options, extreme mode!\n");
                cfg->debug_xmode = true;
                break;
            case 't': /* --print-time */
                cfg->print_time = true;
                break;
            case 'p': /* --pid-file */
                if (optarg)
                    strncpy(cfg->pid_file, optarg, sizeof(cfg->pid_file));

                D_DEBUG0("PID file %s\n", cfg->pid_file);
                break;
            case 'v': /* --version */
                softgred_print_version();
                exit(EXIT_SUCCESS);
            default:
                softgred_print_usage(argv);
                exit(EXIT_SUCCESS);
        }
    }

    return 1;
}

int
softgred_config_load_iface(const char *ifname,
                           struct softgred_config *cfg)
{
    struct sockaddr_in *sin = &cfg->priv.ifname_saddr;
    struct ifaddrs *ifap, *ifa;
    size_t ifacen = strnlen(ifname, SOFTGRED_MAX_IFACE);

    assert (ifname != NULL);
    assert (cfg != NULL);
    assert (sin != NULL);

    getifaddrs (&ifap);

    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (!strncmp(ifname, ifa->ifa_name, ifacen))
        {
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                char *addr = inet_ntoa(sa->sin_addr);
                cfg->ifname = ifname;

                strncpy(cfg->priv.ifname_ip, addr, SOFTGRED_MAX_IFACE);
                memcpy(sin, sa, sizeof(struct sockaddr_in));

                break;
            }
        }
    }

    freeifaddrs(ifap);

    if (!cfg->ifname)
    {
        fprintf(stderr, "** impossible to listening in '%s' network interface, exiting...\n", ifname);
        return 0;
    }

    return 1;
}

int
softgred_config_load_attach(const char *arg,
                            struct softgred_config *cfg)
{
    char *tmp = (char *)arg;
    const char *str_vlan_id = strtok((char *)arg, "@");
    const char *br_name = strtok(NULL, "@");
    size_t br_len = (br_name != NULL) ? strnlen(br_name, 64) : 0;
    uint16_t vlan_id;
    uint8_t pos = cfg->bridge_slot;

    // have args?
    if (!str_vlan_id || !br_name)
    {
        fprintf(stderr, "** error! wrong argument! expected is vlan_id@bridge-to-attach, eg.: -a 10@br-vlan123\n");
        return 0;
    }

    // vlan validate
    vlan_id = strtol(str_vlan_id, NULL, 10);
    if (vlan_id < 1 || vlan_id > 4096)
    {
        fprintf(stderr, "** error! The argument '%s' is a wrong vlan id, exiting...\n", str_vlan_id);
        return 0;
    }

    // bridge validate, expected: "<name><number>" <= SOFTGRED_MAX_IFACE
    if (br_len < 3)
    {
        fprintf(stderr, "** error! The argument '%s' (%ld) is a wrong bridge name. (len >= 3 && len <= %d)\n", 
                                                        br_name, br_len,  SOFTGRED_TUN_PREFIX_MAX);
        return 0;
    }

    if (if_nametoindex(br_name) < 1) // TODO: Change for validate if is a real bridge-interface.
    {
        fprintf(stderr, "** error! The bridge '%s' don't exist in your system! try to create, eg.: brctl addbr %s\n",
                            br_name, br_name);
        return 0;
    }

    if (pos >= SOFTGRED_MAX_ATTACH)
    {
        fprintf(stderr, "** error! The maximum number of slots was reached.\n");
        return 0;
    }

    // adding arguments
    D_DEBUG1("Loading the argument '%s' { .vlan_id='%d', .br_iface='%s' }\n", tmp, vlan_id, br_name);
    cfg->bridge[pos].ifname = br_name;
    cfg->bridge[pos].vlan_id = vlan_id;
    cfg->bridge_slot += 1;

    return 1;
}

void
softgred_print_version()
{
    printf("%s By Jorge Pereira <jpereiran@gmail.com>\n", PACKAGE_STRING);
    printf("Latest Build:    %s - %s\n", __TIME__, __DATE__);
    printf("Project Website: %s\n", PACKAGE_URL);
    printf("Bug Report:      %s\n", PACKAGE_BUGREPORT);
    printf("GIT Branch:      %s\n", CURRENT_BRANCH);
    printf("GIT Commit:      %s\n", CURRENT_COMMIT);
}

void
softgred_print_usage(char *argv[])
{
    const char *arg0 = basename (argv[0]);

    printf("Usage: %s [-t] [-fxdvh] [ -i interface ] [ -P tun_prefix ] [-p pid-file ]\n" \
           "                 [ -a vlan_id1@bridge0 -a vlan_id2@bridge1 ... ]\n" \
           " OPTIONS\n" \
           "\n" \
           "   -i, --iface          Network interface to listen GRE packets, e.g: -i eth0\n" \
           "   -a, --attach         Name of vlan/bridge to be attached, e.g: 10@br-vlan2410\n" \
           "   -P, --tunnel-prefix  Prefix name to be used in gre-network-interfaces, default e.g: %sN (N num. of instance)\n" \
           "   -p, --pid-file       Path of pid-file, default in "SOFTGRED_PIDDIR"/"PACKAGE".pid\n" \
           "   -f, --foreground     Run in the foreground\n"  \
           "   -h, --help           Display this help text\n" \
           "   -d, --debug          Enable debug mode. e.g: -dd (more debug), -ddd (crazy debug level)\n"      \
           "   -x, --xdebug         Enable debug in 'extreme mode'. (enable all environments variables)\n"      \
           "   -t, --print-time     Show the current time in debug/messages.\n"      \
           "   -v, --version        Display the %s version\n" \
           "\n" \
           " EXAMPLE\n" \
           "\n" \
           "Example: We have a interface 'eth0' there is a 'endpoint' of GRE tunnel, and we have a\n" \
           "trunk interface 'eth1'. that can reach many vlans, like 2420,2421 and 2422. Now, we would\n" \
           "like to attach with some vlans over GRE tunnel. \n" \
           "\n" \
           " -> Create a bridge\n" \
           " # ip link add br-vlan2420 type bridge\n" \
           " # ip link add br-vlan2421 type bridge\n" \
           " # ip link add br-vlan2422 type bridge\n" \
           "\n" \
           " -> Create the local VLANs\n" \
           " # ip link add link eth1 name eth1.2420 up type vlan id 2420\n" \
           " # ip link add link eth1 name eth1.2421 up type vlan id 2421\n" \
           " # ip link add link eth1 name eth1.2422 up type vlan id 2422\n" \
           "\n" \
           " -> Attach the local VLANs with the bridges\n" \
           " # ip link set eth1.2420 master br-vlan2420\n" \
           " # ip link set eth1.2421 master br-vlan2421\n" \
           " # ip link set eth1.2422 master br-vlan2422\n" \
           "\n" \
           " -> Now, we can begin the provision!\n" \
           "\n" \
           " # %s -i eth0 -a 10@br-vlan2420 -a 11@br-vlan2421 -a 12@br-vlan2422\n" \
           "\n" \
           " -> Conclusion\n" \
           "\n" \
           " Everything that arrive in GRE tunnel, will be forwarded to internals VLANs/bridge.\n" \
           "\n", arg0, SOFTGRED_TUN_PREFIX, arg0, arg0);
}

int
softgred_config_create_pid_file(int pid)
{
    struct softgred_config *cfg = softgred_config_get_ref();
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

    if (!cfg->pid_file[0])
    {
        snprintf(cfg->pid_file, sizeof(cfg->pid_file), "%s/%s.pid",
                SOFTGRED_PIDDIR, PACKAGE);
    }

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

    snprintf(buf, sizeof(buf), "%ld\n", pid);
    if (write(fd, buf, strlen(buf)) != strlen(buf))
    {
        D_CRIT("Could not write file '%s'", cfg->pid_file);
        return -1;
    }

    return 0;
}


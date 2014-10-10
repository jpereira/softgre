/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */

#define _GNU_SOURCE
#include "general.h"
#include "softgred.h"
#include "iface_ebtables.h"
#include "softgred_config.h"

#include "log.h"
#include "payload.h"
#include "helper.h"
#include "provision.h"

void
softgred_sig_handler(int signo)
{
    switch(signo)
    {
        case SIGSEGV: {
            helper_print_stacktrace();
            exit(EXIT_FAILURE);
        }
        break;

        case SIGINT:
        case SIGTERM: {
            D_INFO("Ooops! received %s, cleaning & leaving...\n", strsignal(signo));

            softgred_end();
            exit(EXIT_FAILURE);
        }
        break;

        case SIGUSR1: {
            D_INFO("Received SIGUSR1, unprovision all!\n");

            /* unprovisione all interfaces */
            provision_delall();
        }
        break;

        case SIGUSR2: {
            struct softgred_config *cfg = softgred_config_get();
            hash_statistics_t st;

            D_INFO("Received SIGUSR2, show statistics!\n");

            if (hash_get_statistics(cfg->table, &st) < 0)
            {
                D_CRIT("Problems with hash_get_statistics()\n");
                return;
            }

            D_INFO("Statistics for table (%p) hash:{ accesses=%ld, collisions=%ld }, table:{ expansions=%ld, contractions=%ld }\n",
                        cfg->table, st.hash_accesses, st.hash_collisions, st.table_expansions, st.table_contractions);
        }
        break;
    }
}

int
softgred_init()
{
    D_DEBUG1("Initializing...\n");

    helper_enable_coredump();

    helper_enable_high_priority();

    /* registering signals */
    signal(SIGINT, softgred_sig_handler);
    signal(SIGTERM, softgred_sig_handler);
    signal(SIGUSR1, softgred_sig_handler);
    signal(SIGUSR2, softgred_sig_handler);
    signal(SIGSEGV, softgred_sig_handler);

    if (softgred_config_init() < 0)
        return -1;

    if (iface_ebtables_init() < 0)
        return -1;

    if (payload_loop_init() != 0)
    {
        D_CRIT("Problems with payload_loop_init()\n");
        softgred_end ();
        return -1;
    }

    return 0;
}

void
softgred_end()
{
    D_DEBUG1("Finalizing...\n");

    payload_loop_end ();

    /* unprovisione all interfaces */
    provision_delall();

    /* reset firewall rules */
    iface_ebtables_end();

    /* release config */
    softgred_config_end();

    /* syslog */
    log_end();
}

int
main (int argc,
      char *argv[])
{
    struct softgred_config *cfg = softgred_config_get();

    if (argc < 2)
    {
        softgred_print_usage(argv);
        exit(EXIT_SUCCESS);
    }

    if (!softgred_config_load_cli(argc, argv))
    {
        fprintf(stderr, "%s: Invalid options!\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* sorry, only root is welcome ... */
    if (getuid() != 0)
    {
        fprintf(stderr, "%s: Sorry, Can't run with different user of root!\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* prepare.. */
    setlocale(LC_CTYPE, "");
    umask(0122);

    /* starting log */
    log_init();

    /* Check debug level */
    if (cfg->debug_mode > 0)
    {
        D_INFO("** SoftGREd %s (Build %s - %s) - Daemon Started **\n", PACKAGE_VERSION, __TIME__, __DATE__);

        if (cfg->debug_mode > DEBUG_MAX_LEVEL)
        {
            fprintf(stderr, "*** Ops!! the maximum of debug level is %d (-ddd).\n", DEBUG_MAX_LEVEL);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "*** Entering in debug mode with level %d! ***\n", cfg->debug_mode);
    }

    /* foreground || background */
    if (!cfg->is_foreground)
    {
        pid_t child;

        if ((child = fork()) < 0)
        {
            fprintf(stderr, "fork(): error, failed fork! [%s]\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (child > 0)
        {
            D_INFO("[pid=%d] Daemon SoftGREd was launched in background with success!\n", child);
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0)
        {
            D_CRIT("setsid(): returned error [%s]\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        signal(SIGCHLD,SIG_IGN);
        signal(SIGHUP, SIG_IGN);
    }

    if (chdir("/") < 0)
    {
        D_CRIT("chdir() returned error\n");
        exit(EXIT_FAILURE);
    }

    D_INFO("Listening GRE packets in '%s/%s' [args is foreground=%d tunnel_prefix=%s]\n",
        cfg->ifname, cfg->priv.ifname_ip, cfg->is_foreground, cfg->tunnel_prefix
    );

    /* pre-run */
    softgred_init();

    /* busyloop */
    D_DEBUG1("Entering main loop\n");
    payload_loop_run ();

    /* cleanup */
    softgred_end ();

    D_DEBUG1("Capture complete, exiting.\n");

    return EXIT_SUCCESS;
}


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
        case SIGINT:
        case SIGTERM:
            fprintf(stderr, "Ooops! received %s, cleaning & leaving...\n", strsignal(signo));
            
            softgred_end();
            exit(EXIT_FAILURE);
            break;

        case SIGUSR1:
        case SIGUSR2:
            D_INFO("Received SIGUSR1, unprovision all!\n");
            /* unprovisione all interfaces */
            provision_delall();
            break;
    }
}

int
softgred_init()
{
    log_init();

    softgred_config_init();

    iface_ebtables_init();

    if (!payload_loop_init())
        softgred_end ();

    return EXIT_SUCCESS;
}

void
softgred_end()
{
    payload_loop_end ();

    /* unprovisione all interfaces */
    provision_delall();

    //iface_ebtables_end();

    softgred_config_end();

    /* syslog */
    log_end();
}

int
main (int argc,
      char *argv[])
{
    struct softgred_config *cfg = softgred_config_get();
    pid_t pid, sid;

    if (argc < 2)
    {
        softgred_print_usage(argv);
        exit(EXIT_SUCCESS);
    }

    /* prepare.. */
    setlocale(LC_CTYPE, "");
    umask(0122);

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

    /* Check debug level */
    if (cfg->debug_mode > 0)
    {
        D_INFO("** SoftGREd %s (Build %s - %s) - Daemon Started **\n",
                                            PACKAGE_VERSION, __TIME__, __DATE__);

        if (cfg->debug_mode > DEBUG_MAX_LEVEL)
        {
            fprintf(stderr, "*** Ops!! the maximum of debug level is %d (-ddd).\n", DEBUG_MAX_LEVEL);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "*** Entering in debug mode with level %d! ***\n", cfg->debug_mode);
    }

    /* registering signals */
    signal(SIGINT, softgred_sig_handler);
    signal(SIGTERM, softgred_sig_handler);
    signal(SIGUSR1, softgred_sig_handler);
    signal(SIGUSR2, softgred_sig_handler);

    D_INFO("Listening GRE packets in '%s/%s' [args is foreground=%d tunnel_prefix=%s]\n",
        cfg->ifname, cfg->priv.ifname_ip, cfg->is_foreground, cfg->tunnel_prefix
    );

    if (payload_loop_init () != EXIT_SUCCESS)
    {
        D_CRIT("Problems with payload_check(%s), exiting...\n", cfg->ifname);
        exit(EXIT_FAILURE);
    }

    /* foreground || background */
    if (!cfg->is_foreground)
    {
        pid = fork();
        if (pid < 0)
            exit(EXIT_FAILURE);
        if (pid > 0)
            exit(EXIT_SUCCESS);

        sid = setsid();
        if (sid < 0)
        {
            D_CRIT("setsid() returned error\n");
            exit(EXIT_FAILURE);
        }

        char *directory = "/";

        if ((chdir(directory)) < 0)
        {
            D_CRIT("chdir() returned error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* pre-run */
    softgred_init();

    /* busyloop */
    D_DEBUG1("Entering main loop\n");
    payload_loop_run ();

    /* cleanup */
    softgred_end ();

    D_DEBUG1("Capture complete.\n");
    D_DEBUG1("exiting\n");

    return 0;
}


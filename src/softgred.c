/**
 * Copyright (C) 2014 Jorge Pereira <jpereiran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define _GNU_SOURCE
#include "general.h"
#include "softgred.h"
#include "softgred_config.h"

#include "log.h"
#include "payload.h"
#include "helper.h"
#include "provision.h"

struct option long_opts[] = {
    { "foreground",    no_argument,       NULL, 'f'},
    { "iface",         required_argument, NULL, 'i'},
    { "tunnel-prefix", optional_argument, NULL, 'p'},
    { "attach",        optional_argument, NULL, 'a'},
    { "debug",         no_argument,       NULL, 'd'},
    { "help",          no_argument,       NULL, 'h'},
    { "version",       no_argument,       NULL, 'v'},
    { NULL,            0,                 NULL,  0 }
};

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

    /* syslog */
    log_end();
}

int
main (int argc,
      char *argv[])
{
    struct softgred_config *cfg = softgred_config_get();
    pid_t pid, sid;

    if (argc < 3)
    {
        softgred_print_usage(argv);
        exit(EXIT_SUCCESS);
    }

    /* prepare.. */
    setlocale(LC_CTYPE, "");
    umask(0037);

    /* sorry, only root is welcome ... */
    if (getuid() != 0) {
        fprintf(stderr, "%s: Sorry, Can't run with different user of root!\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* parsing arguments ... */
    while (true)
    {
        int c = getopt_long(argc, argv, "dfhvi:p:a:", long_opts, NULL);
        if (c == EOF)
            break;

        switch (c) {
            case 'f': /* --foreground */
                cfg->is_foreground = true;
                break;
            case 'p': /* --tunnel-prefix */
                cfg->tunnel_prefix = optarg;
                break;
            case 'i': /* --iface */
                cfg->ifname = optarg;
                softgred_config_load_iface(cfg->ifname, cfg);
                break;
            case 'a': /* --attach */
                if (softgred_config_load_attach(optarg, cfg) != EXIT_SUCCESS)
                    exit(EXIT_FAILURE);
                break;
            case 'h': /* --help */
                softgred_print_usage(argv);
                exit(EXIT_SUCCESS);
                break;
            case 'd': /* --debug */
                cfg->debug_mode += 1 ;
                break;
            case 'v': /* --version */
                softgred_print_version();
                exit(EXIT_SUCCESS);
            default:
                softgred_print_usage(argv);
                exit(EXIT_FAILURE);
        }
    }

    // Check debug level
    if (cfg->debug_mode > 0)
    {
        if (cfg->debug_mode > DEBUG_MAX_LEVEL)
        {
            fprintf(stderr, "*** Ops!! the maximum of debug level is %d (-ddd).\n", DEBUG_MAX_LEVEL);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "*** Entering in debug mode with level %d! ***\n", cfg->debug_mode);
    }

    /* begin */
    D_INFO("** Daemon Started **\n");
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

    /* registering signals */
    signal(SIGINT, softgred_sig_handler);
    signal(SIGTERM, softgred_sig_handler);
    signal(SIGUSR1, softgred_sig_handler);

    /* pre-run */
    softgred_init();

    /* busyloop */
    D_DEBUG("Entering main loop\n");
    payload_loop_run ();

    /* cleanup */
    softgred_end ();

    D_DEBUG("Capture complete.\n");
    D_DEBUG("exiting\n");

    return 0;
}


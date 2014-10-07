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
#include "iface_ebtables.h"
#include "log.h"

void
iface_ebtables_run (const char *chain)
{
    char cmd[512];

    sprintf(cmd, "ebtables -F\n");
    system(cmd);

    sprintf(cmd, "ebtables -P FORWARD %s\n", chain);
    system(cmd);
}

int
iface_ebtables_init()
{
    iface_ebtables_run("DROP");

    return 0;
}

int
iface_ebtables_allow (const struct ether_addr *addr)
{
    char *mac = ether_ntoa(addr);
    char cmd[512];

    D_DEBUG3("allowing mac %s\n", mac);

    sprintf(cmd, "ebtables -I FORWARD 1 -d %s -s Unicast -j ACCEPT\n", mac);
    system(cmd);

    sprintf(cmd, "ebtables -I FORWARD 1 -s %s -d Broadcast -j ACCEPT\n", mac);
    system(cmd);

    return 0;
}

int
iface_ebtables_end ()
{
    iface_ebtables_run("ACCEPT");

    return 0;
}

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

    sprintf(cmd, EBTABLES_BIN" -F\n");
    system(cmd);

    sprintf(cmd, EBTABLES_BIN" -P FORWARD \"%s\"\n", chain);
    system(cmd);
}

int
iface_ebtables_init()
{
    iface_ebtables_run("DROP");

    return 0;
}

int
iface_ebtables_set (const char *target,
                    const char *chain,
                    const char *in_face,
                    const char *src_mac)
{
    char cmd[512];

    D_DEBUG3("talking to Kernel '%s' the '%s' over '%s' by src-mac://%s to anywhere\n", 
                                                                    target, chain, in_face, src_mac);

    sprintf(cmd, EBTABLES_BIN" -I \"%s\" 1 -d \"%s\" -j \"%s\"\n", chain, src_mac, target);
    system(cmd);

    sprintf(cmd, EBTABLES_BIN" -I \"%s\" 1 -s \"%s\" -j \"%s\"\n", chain, src_mac, target);
    system(cmd);

    return 0;
}

int
iface_ebtables_end ()
{
    iface_ebtables_run("ACCEPT");

    return 0;
}


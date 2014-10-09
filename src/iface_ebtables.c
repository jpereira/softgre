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
#include "iface_ebtables.h"
#include "log.h"

void
iface_ebtables_run (const char *chain)
{
    helper_system(false, "%s -F", EBTABLES_BIN);
    helper_system(false, "%s -P FORWARD \"%s\"", EBTABLES_BIN, chain);
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
    D_DEBUG3("talking to Kernel '%s' the '%s' over '%s' by src-mac://%s to anywhere\n", 
                                                                    target, chain, in_face, src_mac);

    helper_system(false, "%s -I \"%s\" 1 -d \"%s\" -j \"%s\"", EBTABLES_BIN, chain, src_mac, target);
    helper_system(false, "%s -I \"%s\" 1 -s \"%s\" -j \"%s\"", EBTABLES_BIN, chain, src_mac, target);

    return 0;
}

int
iface_ebtables_end ()
{
    iface_ebtables_run("ACCEPT");

    return 0;
}


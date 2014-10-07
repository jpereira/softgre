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

#ifndef HELPER_H_
#define HELPER_H_

#include "general.h"
#include "softgred_config.h"

void
helper_print_payload (const u_char *payload,
                      size_t len);

void
helper_print_hex2ascii (const u_char *payload,
                        size_t len,
                        int offset);

#endif /*HELPER_H_*/


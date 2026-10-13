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

#ifndef SOFTGRED_LOG_H
#define SOFTGRED_LOG_H
#include "general.h"
void log_message(int level, const char *format, ...) G_GNUC_PRINTF(2, 3);
#define D_INFO(...) log_message(5, __VA_ARGS__)
#define D_WARNING(...) log_message(5, __VA_ARGS__)
#define D_CRIT(...) log_message(5, __VA_ARGS__)
#define D_DEBUG0(...) log_message(1, __VA_ARGS__)
#define D_DEBUG1(...) log_message(1, __VA_ARGS__)
#define D_DEBUG2(...) log_message(2, __VA_ARGS__)
#define D_DEBUG3(...) log_message(3, __VA_ARGS__)
#endif

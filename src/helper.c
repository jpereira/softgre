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

#include "helper.h"
#include "log.h"
#include "general.h"

/*
 * print data in rows of 16 bytes: offset   hex   ascii
 */
void
helper_print_hex2ascii(const u_char *payload,
					   int len,
					   int offset)
{
    int i;
    int gap;
    const u_char *ch;

    /* offset */
    printf("%05d   ", offset);
    
    /* hex */
    ch = payload;
    for(i = 0; i < len; i++)
	{
        printf("%02x ", *ch);
        ch++;
        /* print extra space after 8th byte for visual aid */
        if (i == 7)
            printf(" ");
    }
    /* print space to handle line less than 8 bytes */
    if (len < 8)
        printf(" ");
    
    /* fill hex gap with spaces if not full line */
    if (len < 16)
	{
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");
    
    /* ascii (if printable) */
    ch = payload;
    for(i = 0; i < len; i++)
	{
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }

    printf("\n");
}

/*
 * print packet payload data (avoid printing binary data)
 */
void
helper_print_payload (const u_char *payload,
                      int len)
{
    int len_rem = len;
    int line_width = 16;            /* number of bytes per line */
    int line_len;
    int offset = 0;                 /* zero-based offset counter */
    const u_char *ch = payload;

    if (len <= 0)
        return;

    D_DEBUG3("<print log payload=%#08x (%d)>\n", payload, len);

    /* data fits on one line */
    if (len <= line_width)
	{
        helper_print_hex2ascii(ch, len, offset);
        D_DEBUG3("</payload log>\n");
        return;
    }

    /* data spans multiple lines */
    for (;;)
	{
        /* compute current line length */
        line_len = line_width % len_rem;
        /* print line */
        helper_print_hex2ascii(ch, line_len, offset);
        /* compute total remaining */
        len_rem = len_rem - line_len;
        /* shift pointer to remaining bytes to print */
        ch = ch + line_len;
        /* add offset */
        offset = offset + line_width;
        /* check if we have line width chars or less */
        if (len_rem <= line_width)
		{
            /* print last line and get out */
            helper_print_hex2ascii(ch, len_rem, offset);
            break;
        }
    }

    D_DEBUG3("</payload log>\n");
}

void
helper_macether2tostr(char *buf, u_int8_t **ether)
{
    u_int8_t *ptr = ether;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);

}


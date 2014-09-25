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
#include "payload.h"
#include "helper.h"
#include "log.h"
#include "provision.h"
#include "softgred_config.h"

struct payload_config *
payload_config_get()
{
    static struct payload_config obj;
    return &obj;
}

int
payload_loop_init ()
{
    struct softgred_config *cfg = softgred_config_get();
    struct payload_config *pl_cfg = payload_config_get();

    assert (cfg->ifname != NULL);
	
	/* get network number and mask associated with capture device */
	if (pcap_lookupnet(cfg->ifname, &pl_cfg->net, &pl_cfg->mask, pl_cfg->errbuf) == -1)
    {
		fprintf(stderr, "Couldn't get netmask for device %s: %s\n",
                    cfg->ifname, pl_cfg->errbuf);
		pl_cfg->net = 0;
		pl_cfg->mask = 0;
	}

    return EXIT_SUCCESS;
}

int
payload_loop_run()
{
    struct softgred_config *cfg = softgred_config_get();
    struct payload_config *pl_cfg = payload_config_get();
    char filter_exp[] = "proto gre";

	/* open capture device */
	pl_cfg->handle = pcap_open_live (cfg->ifname, SNAP_LEN, 1, 1000, pl_cfg->errbuf);
	if (pl_cfg->handle == NULL)
    {
		fprintf(stderr, "Couldn't open device %s: %s\n", cfg->ifname, pl_cfg->errbuf);
		return EXIT_FAILURE;
	}

	/* make sure we're capturing on an Ethernet device [2] */
	if (pcap_datalink (pl_cfg->handle) != DLT_EN10MB)
    {
		fprintf(stderr, "%s is not an Ethernet\n", cfg->ifname);
		return EXIT_FAILURE;
	}

	/* compile the filter expression */
	if (pcap_compile (pl_cfg->handle, &pl_cfg->fp, filter_exp, 0, pl_cfg->net) == -1)
    {
		fprintf(stderr, "Couldn't parse filter %s: %s\n",
		    filter_exp, pcap_geterr(pl_cfg->handle));
		return EXIT_FAILURE;
	}

	/* apply the compiled filter */
	if (pcap_setfilter (pl_cfg->handle, &pl_cfg->fp) == -1)
    {
		fprintf(stderr, "Couldn't install filter %s: %s\n",
		    filter_exp, pcap_geterr(pl_cfg->handle));
		return EXIT_FAILURE;
	}

	/* now we can set our callback function */
    if (pcap_loop(pl_cfg->handle, -1 /* infinity */, payload_got_packet, NULL) != 0)
    {
       D_WARNING("Something happens with pcap_loop(), err='%s'\n", pcap_geterr(pl_cfg->handle));
    }

    return 1;
}

void
payload_loop_end ()
{
    struct payload_config *pl_cfg = payload_config_get();

    /* pcap */
  	pcap_freecode(&pl_cfg->fp);
    
    if (pl_cfg->handle) {
        pcap_breakloop(pl_cfg->handle);
    	pcap_close(pl_cfg->handle);
    }
}

void
payload_got_packet (u_char* UNUSED(args),
                    const struct pcap_pkthdr* UNUSED(header),
                    const u_char *packet)
{
    struct softgred_config* cfg = softgred_config_get();
    const struct tunnel_config  *tun_cfg;
    struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;
	const struct payload_ip *ip = NULL; /* The IP header */
	int size_ip;

	/* define/compute ip header offset */
	ip = (struct payload_ip*)(packet + SIZE_ETHERNET);
	size_ip = (IP_HL(ip) * 4);
	if (size_ip < 20)
    {
		printf("payload_got_packet(): Invalid IP header length: %u bytes\n", size_ip);
		return;
	}
	
	/* payload is 'gre'? */
	if (ip->ip_p != IPPROTO_GRE)
    {
        printf("payload_got_packet(): arrives non-gre packet '%#x', leaving...", ip->ip_p);
		return;
	}

	/*
	 *  OK, this packet is GRE
	 */
    if (ip->ip_src.s_addr == ip_local->s_addr)
    {
        return;
    }

    if (provision_is_exist (&ip->ip_src))
    {
        //D_DEBUG("The client %s is already provisioned, leaving...\n", inet_ntoa(ip->ip_src));
        return;
    }
    
    if (provision_add (&ip->ip_src, &tun_cfg) == false)
    {
        printf("Problems with provision_add()\n");
        return;
    }

    D_DEBUG("The client %s in %s was provisioned with %s with success!\n", 
                inet_ntoa(tun_cfg->ip_remote), cfg->ifname, tun_cfg->ifgre);

    return;
}


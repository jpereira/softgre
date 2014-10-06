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
        D_WARNING("Couldn't get netmask for device %s: %s\n", cfg->ifname, pl_cfg->errbuf);
        return EXIT_FAILURE;
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
        D_CRIT("Couldn't open device %s: %s\n", cfg->ifname, pl_cfg->errbuf);
        return EXIT_FAILURE;
    }

    /* make sure we're capturing on an Ethernet device [2] */
    if (pcap_datalink (pl_cfg->handle) != DLT_EN10MB)
    {
        D_CRIT("%s is not an Ethernet\n", cfg->ifname);
        return EXIT_FAILURE;
    }

    /* compile the filter expression */
    if (pcap_compile (pl_cfg->handle, &pl_cfg->fp, filter_exp, 0, pl_cfg->net) == -1)
    {
        D_CRIT("Couldn't parse filter %s: %s\n",
            filter_exp, pcap_geterr(pl_cfg->handle));
        return EXIT_FAILURE;
    }

    /* apply the compiled filter */
    if (pcap_setfilter (pl_cfg->handle, &pl_cfg->fp) == -1)
    {
        D_CRIT("Couldn't install filter %s: %s\n",
            filter_exp, pcap_geterr(pl_cfg->handle));
        return EXIT_FAILURE;
    }

    /* now we can set our callback function */
    if (pcap_loop(pl_cfg->handle, -1 /* infinity */, payload_handler_packet, NULL) != 0)
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
    
    if (pl_cfg->handle)
    {
        pcap_breakloop(pl_cfg->handle);
        pcap_close(pl_cfg->handle);
    }
}

void
payload_handler_packet (u_char* UNUSED(args),
                        const struct pcap_pkthdr* pkthdr,
                        const u_char* packet)
{
    const struct tunnel_config* tun_cfg;
    struct softgred_config* cfg = softgred_config_get();
    struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;
    char ether_dhost[20];
    char ether_shost[20];

    struct ether_header *ether = (struct ether_header *)packet;

    // only IPv4!
    uint16_t ether_type = ntohs (ether->ether_type); 
    if (ether_type != ETHERTYPE_IP && ether_type != ETHERTYPE_VLAN)
    {
        D_WARNING("The packet (%04x) is unsupported!\n", ether_type);
        return;
    }

    if (ether_type == ETHERTYPE_VLAN)
    {
        D_WARNING("arrive VLAN packet in main iface? we don't have idea what need to do!\n");
        return;
    }
    
    // packet tcp full
    const u_char* pkt = (packet + sizeof(struct ether_header));
    struct ip* ip = (struct ip *)pkt;
    size_t pkt_len = ntohs(ip->ip_len);
    if (pkt_len < sizeof(struct ip))     /* check to see we have a packet of valid length */
    {
        printf("truncated ip %d\n", pkt_len);
        return NULL;
    }

    /* payload is 'gre'? */
    if (ip->ip_p != IPPROTO_GRE)
    {
        D_CRIT("payload_handler_packet(): arrives non-gre packet '%#x', leaving...", ip->ip_p);
        return;
    }

    // can't self provision!
    if (ip->ip_src.s_addr == ip_local->s_addr)
    {
        return;
    }

    printf("\n");
    printf("####################################################\n");
    printf("## Packets FULL.\n");
    helper_macether2tostr(ether_shost, &ether->ether_shost);
    helper_macether2tostr(ether_dhost, &ether->ether_dhost);
	printf("       From: %s (%#08x) mac='%s'\n", inet_ntoa(ip->ip_src), ip->ip_src, ether_shost);
	printf("         To: %s (%#08x) mac='%s'\n", inet_ntoa(ip->ip_dst), ip->ip_dst, ether_dhost);
    printf("helper_print_payload(pkt=%#08x pkt_len=%ld)\n", pkt, pkt_len);
    helper_print_payload(pkt, pkt_len);

    // gre packet
    const u_char* pktgre = (pkt + GRE_LENGHT + sizeof (struct ip));
    struct ether_header *ether_gre = (struct ether_header *)(pkt + GRE_LENGHT + sizeof (struct ip));
    struct vlan_tag *vlan;
    size_t pad = 0;
    
    ether_type = ntohs (ether_gre->ether_type); 
    if (ether_type != ETHERTYPE_IP && ether_type != ETHERTYPE_VLAN)
    {
        D_WARNING("The packet (%04x) is unsupported!\n", ether_type);
        return;
    }

    if (ether_type == ETHERTYPE_VLAN)
    {
        vlan = (pkt + GRE_LENGHT + sizeof (struct ip) + sizeof(struct ether_header));
        pad = 4;

        if (vlan->vlan_tpid != ETHERTYPE_VLAN)
        {
            D_WARNING("The packet vlan->vlan_tpid (%04x) is unsupported!\n", vlan->vlan_tpid);
//            return;
        }
    }

    printf("## Packets GRE. (%#04x) pad=%d\n", ether_gre->ether_type, pad);

    struct ip* ip_gre = (struct ip *)(pkt + GRE_LENGHT + sizeof (struct ip) + sizeof(struct ether_header) + pad);
    size_t pktgre_len = ntohs(ip_gre->ip_len);

    uint16_t vlan_id = 0;

    if (ether_type == ETHERTYPE_VLAN)
    {
        vlan_id = ntohs(vlan->vlan_tpid);
        printf("    ETHERTYPE: VLAN (%d) [tpid=%#04x tci=%#04x]\n", vlan_id, vlan->vlan_tci, vlan->vlan_tpid);
    }
    else
        printf("    EHTERTYPE: IP\n");

    helper_macether2tostr(ether_shost, &ether_gre->ether_shost);
    helper_macether2tostr(ether_dhost, &ether_gre->ether_dhost);
	printf("       From: %s (%#08x) mac='%s'\n", inet_ntoa(ip_gre->ip_src), ip_gre->ip_src, ether_shost);
	printf("         To: %s (%#08x) mac='%s'\n", inet_ntoa(ip_gre->ip_dst), ip_gre->ip_dst, ether_dhost);
    printf("helper_print_payload(pktgre=%#08x pktgre_len=%ld)\n", pktgre, pktgre_len);
    //helper_print_payload(pktgre, pktgre_len);

    // if exist, get out!
    if (provision_is_exist (&ip->ip_src))
    {
        //D_DEBUG3("The client %s is already provisioned, leaving...\n", inet_ntoa(ip->ip_src));
        return;
    }
    
    // ok! just do it!
    if (provision_add (&ip->ip_src, &tun_cfg) == false)
    {
        D_CRIT("Problems with provision_add()\n");
        return;
    }

    D_DEBUG1("The client %s in %s was provisioned with %s with success!\n", 
                inet_ntoa(tun_cfg->ip_remote), cfg->ifname, tun_cfg->ifgre);

#if 0

    /* define/compute ip header offset */
    ip = (struct payload_ip*)(packet + SIZE_ETHERNET);
    size_ip = (IP_HL(ip) * 4);
    if (size_ip < 20)
    {
        D_CRIT("payload_handler_packet(): Invalid IP header length: %u bytes\n", size_ip);
        return;
    }
    printf("size_ip=%ld\n", size_ip);

    /* payload is 'gre'? */
    if (ip->ip_p != IPPROTO_GRE)
    {
        D_CRIT("payload_handler_packet(): arrives non-gre packet '%#x', leaving...", ip->ip_p);
        return;
    }

    /*
     *  OK, this packet is GRE
     */


	/* define ethernet header */
	ethernet = (struct payload_ethernet*)(packet);
	
	/* print source and destination IP addresses */
    D_DEBUG1("MAC Adress ether_dhost='%#08x' ether_shost='%#08x'\n", ethernet->ether_dhost, ethernet->ether_shost);
	D_DEBUG1("       From: %s\n", inet_ntoa(ip->ip_src));
	D_DEBUG1("         To: %s\n", inet_ntoa(ip->ip_dst));
	
	/* define/compute tcp payload (segment) offset */
	payload = (u_char *)(packet + SIZE_ETHERNET + size_ip + size_tcp);
	
	/* compute tcp payload (segment) size */
	size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

	if (size_payload > 0)
    {
		D_DEBUG1("   Payload (%d bytes):\n", size_payload);
		//helper_print_payload(payload, size_payload);
	}


#endif
    return;
}


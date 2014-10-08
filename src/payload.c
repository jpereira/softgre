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
    if (pcap_loop(pl_cfg->handle, -1 /* infinity */, payload_handler_packet_cb, NULL) != 0)
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

// TODO: please, refactor this as soon as possible!
void
payload_handler_packet_cb (u_char *UNUSED(args),
                           const struct pcap_pkthdr *UNUSED(pkthdr),
                           const u_char *packet)
{
    const struct tunnel_context *tun;
    register struct softgred_config *cfg = softgred_config_get();
    register struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;

    // only IPv4!
    struct ether_header *ether = (struct ether_header *)&packet[0];
    uint16_t ether_type = ntohs (ether->ether_type); 
    if (ether_type != ETHERTYPE_IP)
    {
        D_WARNING("The packet (%#04x) is unsupported!\n", ether_type);
        return;
    }
   
    // packet tcp full
    const u_char *pkt = (packet + sizeof(struct ether_header));
    struct ip *ip = (struct ip *)pkt;
    size_t pkt_len = ntohs(ip->ip_len);
    if (pkt_len < sizeof(struct ip)) // check to see we have a packet of valid length
    {
        D_CRIT("wrong lenght, may truncated ip %ld?\n", pkt_len);
        return;
    }

    // at first, only "GRE" packets!!
    if (ip->ip_p != IPPROTO_GRE)
    {
        D_CRIT("arrives non-gre packet '%#x', was ignored!", ip->ip_p);
        return;
    }

    // self provision? no way!
    if (ip->ip_src.s_addr == ip_local->s_addr)
        return;

    // dump/debug message
    if (cfg->debug_packet)
    {
        printf("\n");
        printf("####################################################\n");
        printf("## Packet TCP (complete)\n");
	    printf("   <-  From: %s (%s)\n", inet_ntoa(ip->ip_src), ether_ntoa((const struct ether_addr *)ether->ether_shost));
    	printf("   ->    To: %s (%s)\n", inet_ntoa(ip->ip_dst), ether_ntoa((const struct ether_addr *)ether->ether_dhost));
        //helper_print_payload(pkt, pkt_len);
    }

    // gre packet
    //const u_char* pktgre = (pkt + GRE_LENGHT + sizeof (struct ip));
    struct ether_header *ether_gre = (struct ether_header *)(pkt + GRE_LENGHT + sizeof (struct ip));
    size_t pad = 0;
    uint16_t vlan_id = 0;

    ether_type = ntohs (ether_gre->ether_type); 
    if (ether_type != ETHERTYPE_IP && ether_type != ETHERTYPE_VLAN)
    {
        //D_WARNING("The packet (%#04x) is unsupported!\n", ether_type);
        return;
    }

    if (ether_type == ETHERTYPE_VLAN)
    {
        uint16_t *pkt_vlan = (uint16_t *)(pkt + GRE_LENGHT + sizeof (struct ip) + sizeof(struct ether_header));
        pad = 4;

        vlan_id = htons(pkt_vlan[0] & 0xfff0);
        ether_type = htons(pkt_vlan[1]); // update ether type
    }

    if (cfg->debug_packet)
    {
        printf(" @GRE ether_type(%#04x) pad=%ld vlan_id=%d\n", ether_type, pad, vlan_id);
    }

    if (ether_type != ETHERTYPE_VLAN && ether_type != ETHERTYPE_IP)
    {
        //D_WARNING("The packet (%#04x) is unsupported!\n", ether_type);
        return;
    }

    struct ip *ip_gre = (struct ip *)(pkt + GRE_LENGHT + sizeof (struct ip) + sizeof(struct ether_header) + pad);
    //size_t pktgre_len = ntohs(ip_gre->ip_len);

    if (cfg->debug_packet)
    {
	    printf("   <-  From: %s (%s)\n", inet_ntoa(ip_gre->ip_src), ether_ntoa((const struct ether_addr *)ether_gre->ether_shost));
	    printf("   ->    To: %s (%s)\n", inet_ntoa(ip_gre->ip_dst), ether_ntoa((const struct ether_addr *)ether_gre->ether_dhost));
        //printf(" @helper_print_payload(pktgre=%#08x, pktgre_len=%ld)\n", pktgre, pktgre_len);
        //helper_print_payload(pktgre, pktgre_len);
    }

    const struct ether_addr *ether_src = (const struct ether_addr *)ether_gre->ether_shost;
    const char *src_mac = ether_ntoa(ether_src);
 
    if (!src_mac)
    {
        D_DEBUG3("invalid mac!\n");
        return;
    }

    // if exist, get out!
    tun = provision_has_tunnel(&ip->ip_src);
    if (tun)
    {
        if (provision_tunnel_has_mac(tun, src_mac) == true)
        {
            //D_DEBUG3("the mac %s is already allowed over %s@%s\n", smac, inet_ntoa(tun->ip_remote), tun->ifgre);
            return;
        }

        if (provision_tunnel_allow_mac(tun, src_mac) != true)
        {
            D_DEBUG3("problems with provision_tunnel_allow_mac(%s)\n", src_mac);
            return;
        }

        D_DEBUG3("the mac address '%s' was allowed\n", src_mac);

        return;
    }
    
    // ok! just do it!
    tun = provision_add (&ip->ip_src);
    if (!tun)
    {
        D_CRIT("Problems with provision_add()\n");
        return;
    }

    if (provision_tunnel_allow_mac(tun, src_mac) != true)
    {
        D_DEBUG3("problems with provision_tunnel_allow_mac(%s)\n", src_mac);
        return;
    }

    D_DEBUG1("[GRE:%s] the client %s was attached in %s\n", 
                cfg->ifname, inet_ntoa(tun->ip_remote), tun->ifgre);

    return;
}

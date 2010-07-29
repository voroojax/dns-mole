/* dns_sniffer.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * $Id$
 */

#include <time.h>
#include <pcap.h>
#include <net/ethernet.h>
#include <stdlib.h>
#include <string.h>

#include "../include/dnsmole.h"
#include "../include/dns_sniffer.h"
#include "../include/error.h"
#include "../include/dns_parser.h"


int sniffer_setup(void *mW) {
	char *dev;
	char errbuf[PCAP_ERRBUF_SIZE];
	
    moleWorld *mWorld = (moleWorld *) mW;

	if(getuid()) {
		return PCAP_ROOT_ERROR;
	}

	/* look up device */
	if (mWorld->interface == NULL) {
		dev = pcap_lookupdev(errbuf);
	}
	else dev = mWorld->interface;
	if (dev == NULL) {
		return PCAP_LOOKUPDEV_ERROR;
	}

	/* ask pcap for the network address and mask of the device */
	bpf_u_int32 maskp;
	bpf_u_int32 netp;
	pcap_lookupnet(dev, &netp, &maskp, errbuf);

	/* open device for reading */
	mWorld->p = pcap_open_live(dev, BUFSIZ, 1, -1, errbuf);
	if (mWorld->p == NULL) {
		return PCAP_OPEN_LIVE_ERROR;
	}

	/* compile the program */
	struct bpf_program filter;
	if (pcap_compile(mWorld->p, &filter, DNS_QUERY_FILTER, 0, netp) == -1) {
		return PCAP_COMPILE_ERROR;
	}

	/* set the compiled program as the filter */
	if (pcap_setfilter(mWorld->p, &filter) == -1) {
		return PCAP_SETFILTER_ERROR;
	}

	/* initial structure */
	
	mWorld->qlist_head = (query *)malloc(sizeof(query));
	memset(mWorld->qlist_head, 0, sizeof(query));
	mWorld->qlist_rear = mWorld->qlist_head;
	
	return 0;
}

/* callback function for pcap */

void _dns_sniffer(int fd, short event, void *arg) {
    struct moleWorld *myMole = (struct moleWorld *) arg;

    evtimer_add(&myMole->recv_ev, &myMole->tv);
   
    if(pcap_dispatch(myMole->p, 0,(void *) pcap_callback,(void *) myMole) < 0){
        fprintf(stderr,"[pcap] pcap_dispatch\n"); exit(EXIT_FAILURE);
    }
}


/* handle every packet capturedmWorld.qlist_head = (query *)malloc(sizeof(query));
    mWorld.qlist_rear; */

void pcap_callback(u_char *args, const struct pcap_pkthdr *pkthdr,
		const u_char *packet) {
	moleWorld *mWorld = (moleWorld *)args;
	unsigned int length = pkthdr->len;
	struct ether_header * ehdr;
	unsigned short ether_type;
	
	ehdr = (struct ether_header *) packet;
	ether_type = ntohs(ehdr->ether_type);
	
	if(ether_type == ETHERTYPE_IP){
		query *q = (query *)malloc(sizeof(query));
		memset(q, 0, sizeof(query));
		
		dns2query((u_char *)packet, pkthdr->len, q);
		q->time = pkthdr->ts.tv_sec;
		query_insert_after(mWorld->qlist_rear, q);
		print(q);
                //q->dname[strlen(q->dname)+1]='\0';
		load_domain(q->dname,mWorld->re,mWorld->root_list,1);
		
	}
}
#include <queue.h>
#include "skel.h"
#include "route_table.h"
#include "arp_table.h"

int arp_t_size, r_t_size;

u_char BROADCAST[] = {-1,-1,-1,-1,-1,-1};

struct route_table *route_table;
struct arp_table *arp_table;
queue queue_packets;

/**
 * 	Trimiterea unui packet.
 *  
 * * Se recalculeaza checksumul, daca este gresit se arunca pachetul.
 * 
 * * Se decrementeaza TTL-ul, daca pachetul are TTL expirat, se trimite un 
 * 	 pachet ICMP la sursa pachetului.
 * 
 * * Se recalculeaza checksum-ul, se stocheaza in headereul pachetului.
 * 
 * * Se cauta intrarea cea mai specifica pe care face match adresa
 * 	 IP destinatie.
 * 
 * * Se cauta adresa MAC a next_hop-ului la care trebuie trimis pachetul
 * 	 in arp_table.
 * 
 * * Se completeaza headerul Ethernet cu MAC-ul interfetei sursei pe
 *   care o sa fie trimis pachetul si cu MAC-ul destinatiei.
 * 	
 * * Se trimite pachetul pe interfata corespunzatoare.
 * 
 */
void forward(packet m) {
	int rc, route, arp;
	uint16_t checksum;

	struct ether_header *eth_hdr = (struct ether_header *)m.payload;
	struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct  ether_header));
	
	checksum = ip_hdr->check;
	ip_hdr->check = 0;

	if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != checksum) {
		return;
	}

	ip_hdr->ttl--;

	if (ip_hdr->ttl < 1) {
		send_icmp(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost, 
					eth_hdr->ether_dhost, 11, 0, m.interface,
					htons(getpid() & 0xFFFF), htons((ip_hdr->ttl - 1) & 0xFFFF));
		return;
	}

	ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));

	route = find_ip(route_table, ip_hdr->daddr, 0, r_t_size - 1);

	if (route == -1) {
		return;
	}
		
	arp = find_mac(arp_table, arp_t_size, route_table[route].next_hop);

	if (arp == -1) {
		return;
	}

	get_interface_mac(route_table[route].interface, eth_hdr->ether_shost);
	memcpy(eth_hdr->ether_dhost, arp_table[arp].mac, sizeof(eth_hdr->ether_dhost));

	rc = send_packet(route_table[route].interface, &m);
	DIE(rc < 0, "send_message");

}

/**
 * Se trimit toate pachetele adaugate in coada.
 */
void send_packets() {
	while (!queue_empty(queue_packets)) {
		void *p = queue_deq(queue_packets);
		packet *m = (packet *)p;
		forward(*m);
	
	}
}

/**
 * Daca pachetul este un pachet de tip ARP_REQUEST destinat routerului
 * se trimite un mesaj de tip ARP_REPLY sursei cu adresa MAC a ruterului pe
 * interfata pe care a venit pachetul. Altfel pachetul se arunca.
 * 
 * Daca pachetul este de tip ARP_REPLY se stocheaza IP-ul si MAC-ul in 
 * tabela ARP a sursei pachetului. Se trimit toate pachetele adaugate
 * in coada.
 */
void recive_arp(packet m) {
	struct ether_header *eth_hdr = (struct ether_header *)m.payload;
	struct arp_header *arp_hdr = parse_arp(m.payload);

	char *ip = get_interface_ip(m.interface);
	uint32_t ip_router = inet_addr(ip);

	if (ntohs(arp_hdr->op) == ARPOP_REQUEST && ip_router == arp_hdr->tpa){
		memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
		get_interface_mac(m.interface, eth_hdr->ether_shost);
		send_arp(arp_hdr->spa, arp_hdr->tpa, eth_hdr, m.interface, htons(ARPOP_REPLY));
	} 

	if (ntohs(arp_hdr->op) == ARPOP_REPLY) {
		add_arp_table(arp_table, arp_t_size, arp_hdr->spa, arp_hdr->sha);
		arp_t_size++;

		send_packets();
	
	}

}

/**
 *  Se trimite un ARP_REQUEST ca broadcast pentru 
 * a receptiona adresa MAC a destinatiei.
 */
void send_arp_request(packet m) {
	int route;

	struct ether_header *eth_hdr = (struct ether_header *)m.payload;
	struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct  ether_header));
	
	route = find_ip(route_table, ip_hdr->daddr, 0, r_t_size - 1);

	if (route == -1) {
		return;

	} 

	get_interface_mac(route_table[route].interface, eth_hdr->ether_shost);
	memcpy(eth_hdr->ether_dhost, BROADCAST, sizeof(BROADCAST));
	eth_hdr->ether_type = htons(ETHERTYPE_ARP); 


	char *ip_interface  = get_interface_ip(route_table[route].interface);

	send_arp(route_table[route].next_hop, inet_addr(ip_interface),
		eth_hdr, route_table[route].interface, htons(ARPOP_REQUEST));
}

/**
 * 		Se parseaza tabela de rutare din fisierul primit
 * 	ca parametru.
 * 
 *		Intr-o bucla while se receptioneaza pachetele.
 * 
 * 		Se verifica daca pachetul are header ARP de tip ARP_REQUESTS
 *  destinat ruterului, se raspunde cu ARP_REPLY. Altfel arunca pachetul.
 *  Daca este de tip ARP_REPLY se stocheaza adresa MAC si IP-ul suresei 
 *  in tabela ARP. Se trimit pachetele din coada.
 * 
 * 		Daca pachetul este de tip ICMP ECHO_REQUEST destinat ruterului, 
 * raspunde cu ECHO_REPLY, altfel arunca pachetul.
 * 
 * 		Daca nu se gaseste o intrare pe care adresa IP destinatie sa faca match
 * in tabela de rutare, se arunca pachetul si se transmite un pachet ICMP de tip
 * HOST_UNREACHABLE sursei pachetului.
 * 	
 *		Daca nu se gaseste IP-ul destintatiei in tabela ARP, se trimite un pachet
 * de tip ARP_REQUESTS, pachetul se adauga in coada si se trimite cand se primeste
 * ARP_REPLY.
 * 		Daca se gaseste IP_ul destintatiei in tabela ARP, pachetul se trimite destinatiei.
 */

int main(int argc, char *argv[])
{
	packet m;
	int rc, route, arp;

	init(argc - 2, argv + 2);

	queue_packets = queue_create();
	
	route_table = malloc( sizeof(struct route_table) * 70000);
	arp_table = malloc( sizeof(struct arp_table) * 10);

	arp_t_size = 0;
	r_t_size = read_rtable(route_table, argv[1]);


	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");

		void *p = (packet *)malloc(sizeof(packet));
		memcpy(p, &m, sizeof (packet));

		struct ether_header *eth_hdr = (struct ether_header *)m.payload;
		struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct ether_header));
		struct arp_header *arp_hdr =  parse_arp(m.payload);
		struct icmphdr *icmp_hdr = parse_icmp(m.payload);
	

		if (arp_hdr != NULL) {
			recive_arp(m);
			continue;
		}

		if (icmp_hdr != NULL && icmp_hdr->type == 8 && icmp_hdr->code ==0) {
			char *ip = get_interface_ip(m.interface);
			uint32_t ip_in = inet_addr(ip);

			if (ip_in == ip_hdr->daddr) {
				send_icmp(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost,
				 			eth_hdr->ether_dhost, 0, 0, m.interface, 
							htons(getpid() & 0xFFFF), htons((ip_hdr->ttl - 1) & 0xFFFF));
				continue;
			}
	
		}
		
		route = find_ip(route_table, ip_hdr->daddr, 0, r_t_size - 1);

		
		if (route == -1) {
		 	send_icmp(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost,
			 		 eth_hdr->ether_dhost, 3, 0, m.interface, 
					 htons(getpid() & 0xFFFF), htons((ip_hdr->ttl - 1) & 0xFFFF));
			continue;
		} 
			
		arp = find_mac(arp_table, arp_t_size, route_table[route].next_hop);
	
	
		if (arp == -1) {
			queue_enq(queue_packets, p);
			send_arp_request(m);	
		} else {
		 	forward(m);
		}
	}


	free(route_table);
	free(arp_table);

}

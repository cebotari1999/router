#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/* Aceasta structura este folosita pentru a stoca adresele MAC si IP
a destinatiilor.*/
struct arp_table{
	uint32_t ip;
	uint8_t mac[6];
};


/**
 *Cautarea adresei MAC pentru un anumit IP in tabela arp.
 */
int find_mac(struct arp_table *arp_table, int size, uint32_t ip) {
    if (size == 0) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (arp_table[i].ip == ip) {
            return i;
        }
    }

    return -1;
}

/** 
 * Adaugarea unui nou IP si adresa MAC a acestuia in tabela arp.
 */
void add_arp_table(struct arp_table *arp_table, int size, uint32_t ip, uint8_t mac[6]) {
    arp_table[size].ip = ip;
    memcpy(arp_table[size].mac, mac, 6);
}

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * Structura folosita pentru a stoca adresele tabela de
 * rutare.
 */

struct route_table {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed));

int sort(const void *a, const void * b) {
	return ((struct route_table *)a)->prefix - ((struct route_table *)b)->prefix;
}

/**
 * Citirea tabelei de rutare dintr-un fisier. 
 * Sortarea acesteia folosit quicksort.
 */
int read_rtable (struct route_table *route_table, char file[10]){
	int size = 0;
	char prefix[20], next_hop[20], mask[20];

    FILE *f = fopen(file, "r");
	DIE(f == NULL, "Error to open file.");

    while (fscanf(f, "%s %s %s %d\n", prefix, 
		next_hop, mask, &route_table[size].interface) != EOF) {
        route_table[size].prefix = inet_addr(prefix);
        route_table[size].next_hop = inet_addr(next_hop);
        route_table[size].mask = inet_addr(mask);
        size++;
    }

    qsort((void *) route_table, size, sizeof(route_table[0]), sort);
    fclose(f);
	return size;
}

/**
 *  Cautarea celui mai mare prefix pe care face match IP-ul primit ca 
 * parametru. Daca nu se gaseste o astfel de intrare, se returneaza -1.
 **/
int find_ip(struct route_table *route_table, uint32_t ip, int left, int right) {

	int midle = 0;

	while(left <= right) {
		midle = (left + right) / 2;
		if((ip & route_table[midle].mask) == route_table[midle].prefix) {
			while ((ip & route_table[midle + 1].mask) == route_table[midle + 1].mask) {
				midle++;
			}
			return midle;
		}

		if((ip & route_table[midle].mask) > route_table[midle].prefix)
			left = midle + 1;

		if((ip & route_table[midle].mask) < route_table[midle].prefix)
			right = midle - 1;

	}

	return -1;
}

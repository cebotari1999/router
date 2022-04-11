# router

Structura route_table este folosita pentru a stoca tabela de rutare a roterului.
Functia read_table se preocupa de parsarea tabelei de rutare din fisierul primit ca
parametru in linia de comanda. Se stocheaza prefixul, next_hop-ul, masca si interfata
pentru fiecare intrare din tabela de rutare a fisierului.

Functia find_ip cauta intrarea cu prefixul cel mai mare din tabela de rutare pe care
face match IP-ul primit ca parametru. Am folosit binary search pentru ca are 
complexitatea O(logn), preventiv am sortat tabela de rutare. Daca nu se gaseste adresa
cautata, functia returneaza -1.

Structura arp_table este folosita pentru a stoca adresele IP si MAC-urile acestora. 
Functia add_arp_table adauga o intrare noua in tabela arp la finalul acesteia. Iar
functia find_mac retuneaza pozitia din tabela la care se gaseste adesa IP primita ca 
parametru. Daca nu contine o astfel de adresa returneaza -1.

La inceputul programului se aloca tabela de rutare si tabela arp. Se parseaza tabela
de rutare din fisierul numele caruia e primit ca parametru. Apoi intr-o bucla while(1) 
se receptioneaza pachete. Se verifica daca pachetul este:

    * pachet ARP:
        - daca este un pachet de tip ARP_REQUEST destinat ruterului, raspunde cu adresa 
          MAC a interfetei ruterului pe care a venit pachetul. Altfel, arunca pachetul;

        - daca este un pachet de tip ARP_REPLY, stocheaza adresa IP si MAC-ul sursei 
          pachetului, se trimit pachetele din coada;

    * pachet ICMP:
        - daca pachetul este pachet ICMP ECHO_REQUEST destinat ruterului, se trimite un 
          pachet ECHO_REPLY sursei pachetului. Daca pachetul e destinat ruterului, dar
          are alt tip, este aruncat;
    
    Altfel, pachetul este redirectionat mai departe.

Se cauta intrarea din tabela de rutare pe care face match IP-ul destinatiei. Daca nu
Se gaseste, pachetul este aruncat, se trimite un pachet de tip ICMP HOST_UNREACHABLE
sursei pachetului.

Se cauta next_hop-ul in tabela arp. Daca nu se gaseste un astfel de IP in tabela arp,
pachetul se adauga in coada, se trimite un ARP_REQUEST destinatiei. Altfel pachetul este
trimis next_hop-ului.

Inainte ca pachetul sa fie trimis, se verifica daca are checksum-ul gresit, pachetul
se arunca. Daca are TTL <= 1, se arunca pachetul, se trimite un pachet ICMP de tip 
TIME_EXPECTED sursei pachetului. Se recalculeaza checksum-ul, se completeaza headerul
ETHERNET a pachetului, cu MAC-ul destinatiei din tabela de rutare si cu MAC-ul sursei
corespunzator interfetei pe care este trimit pachetul.

    

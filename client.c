#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "common.h"


int main(int argc, char *argv[]){
    int sock = 0;
    struct sockaddr_in server_addr;
    Messaggio msg;

    // hai inserito l'ID e il dato da terminale???
    if (argc != 3) {
        printf("Uso: %s <ID_MITTENTE> <DATO_NUMERICO>\n", argv[0]);
        return -1;
    }

    msg.id_mittente = atoi(argv[1]);
    msg.dato = atoi(argv[2]);

    // CReao il socket del client
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERRORE: Creazione socket fallita");
        return -1;
    }

    // Configuro l'indirizzo del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // inet_pton converte l'IP in formato binario di rete
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("ERRORE: Indirizzo IP non valido");
        return -1;
    }

    // connetto al server
    puts("Connessione al server in corso...");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERRORE: Connessione fallita.");
        return -1;
    }

    srand(time(NULL) ^ getpid());
    if (rand() % 100 < 30){
        send(sock, &msg, sizeof(Messaggio), 0);
        struct linger sl;
        sl.l_onoff = 1;   // Abilita illinger
        sl.l_linger = 0;  // Timeout a 0: forza RST (chiusura brusca) invece di FIN
        setsockopt(sock, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
        close(sock);
        exit(1);   
    } else {
        send(sock, &msg, sizeof(Messaggio), 0);
        printf("Messaggio inviato correttamente! ID: %d, DATO: %d\n", msg.id_mittente, msg.dato);
        
        char risposta[64];
        int bytes_ricevuti = recv(sock, risposta, sizeof(risposta) - 1, 0);
        if (bytes_ricevuti > 0){
            risposta[bytes_ricevuti] = '\0';
            printf("Risposta del server: %s\n", risposta);
        } else {
            puts("Il server ha chiuso la connessione.");
        }
    }

    close(sock);
    return 0;
}

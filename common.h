#ifndef COMMON_H
#define COMMON_H

// Scelgo una porta fissa per la comunicazione
#define PORT 8080
// scelgo un limite di connessioni in coda
#define CONNESSIONI_IN_CODA 10

// mess dei produttori per il coordinatore
typedef struct {
    int id_mittente;
    int dato;
} Messaggio;


#endif

#define _POSIX_C_SOURCE 200809L    

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // permette di accedere a funzioni come close()
#include <arpa/inet.h>          // Contine funzioni di rete (Socket, bind, listen, accept, ...)
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "common.h"

// dimensione provvisoria per un piccolo test
#define DIMENSIONE_MAX_LOG 500

// creao un mutex
pthread_mutex_t mutex_per_log = PTHREAD_MUTEX_INITIALIZER;

/* "__thread" sono variabili tls, in ratica sono come delle variabili locali per 
il singolo thread altrimenti se uso una semplice variabile "int" 
la presenza di più client causerebbe la sua sovrascrittura (ID thread)
*/
__thread int id_corrente = -1;
__thread volatile int pipe_rotta = 0;

// aggiungo il flag per l'allarmeù
volatile int allarme_scattato = 0;

// var per SIGINT
volatile int server_attivo =1;
int server_fd;

// var per contare i thread attivi per aspettarli
int thread_attivi = 0;
pthread_mutex_t mutex_contatore = PTHREAD_MUTEX_INITIALIZER;

void incrementa_thread() {
    pthread_mutex_lock(&mutex_contatore);
    thread_attivi++;
    pthread_mutex_unlock(&mutex_contatore);
}

void decrementa_thread() {
    pthread_mutex_lock(&mutex_contatore);
    thread_attivi--;
    pthread_mutex_unlock(&mutex_contatore);
}

void scrivi_log(int id, int dato, const char* evento_speciale) {
    time_t orario_corrente;
    time(&orario_corrente);
    struct tm *info_tempo = localtime(&orario_corrente);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", info_tempo);

    pthread_mutex_lock(&mutex_per_log);

    FILE *f_log = fopen("log.txt", "a");
    if (f_log != NULL) {

        // Estraggo il file descriptor per fcntl
        int fd = fileno(f_log); 
        
        // 2. Lock a livello di Sistema Operativo
        struct flock lock;
        memset(&lock, 0, sizeof(lock));
        lock.l_type = F_WRLCK;    // Lock in scrittura
        lock.l_whence = SEEK_SET; 
        lock.l_start = 0;         
        lock.l_len = 0;           // Tutto il file

        if (fcntl(fd, F_SETLKW, &lock) == -1){
            perror("Errore nell'acquisizione del lock di scrittura");       
        }
        // Zona critica
        if (evento_speciale == NULL) {
            // Scrittura normale: [TIMESTAMP, ID, DATO]
            fprintf(f_log, "[%s, %d, %d]\n", timestamp, id, dato);
        } else {
            // Scrittura disconnessione: [TIMESTAMP, ID, "DISCONNECT"]
            fprintf(f_log, "[%s, %d, %s]\n", timestamp, id, evento_speciale);
        }

        // flush per svuotare il buffer su disco 
        fflush(f_log);
        // fine zona critica
        lock.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &lock) == -1){
            perror("Errore nel rilascio del lock");       
        }

        fclose(f_log);
    } else {
        perror("Errore apertura log");
    }
    
    pthread_mutex_unlock(&mutex_per_log);
}

// tutti gli handler 
void gestore_sigalarm(int sig) {
    (void)sig;
    allarme_scattato = 1;
}

void gestore_sigpipe(int sig) {
    (void)sig;
    pipe_rotta = 1;
}

void gestore_sigint(int sig) {
    (void)sig;
    server_attivo = 0;
}
// fine handler

// Funzione per i thread
void *gestisci_client(void *arg){
    incrementa_thread();
    // socket dato dal main
    int sock = *(int*)arg;
    // libero subito la memoria allocata dal main
    free(arg);
    
    Messaggio msg_ricevuto;
    // leggo il messaggio ricevuto
    ssize_t bytes_letti = read(sock, &msg_ricevuto, sizeof(Messaggio));

    // controllo se ha contenuto
    if (bytes_letti >= 0) {
        if (bytes_letti == 0) {
            // Caso 1: Il client si è connesso ma ha chiuso subito la connessione (0 byte)
            // In sintesi, chiusura pulita del client e invio pacchetto FIN
            printf(" ID thread [%lu] -> Client disconnesso prima di inviare dati.\n", pthread_self());
            
            // Siccome non ha inviato nulla, non Ho il suo ID. 
            // Uso un ID fittizio (-1) per segnalare l'anomalia.
            scrivi_log(-1, 0, "EMPTY_CONNECT_OR_DROP");
            
        } else {
            // Salvo l'ID nella memoria personale del thread in caso di futuri crash
            id_corrente = msg_ricevuto.id_mittente;
        
            printf(" ID thread [%lu], Dati ricevuti -> ID: %d | DATO: %d\n", pthread_self(), msg_ricevuto.id_mittente, msg_ricevuto.dato);
        
            // Uso la funzione helper per scrivere i dati
            scrivi_log(msg_ricevuto.id_mittente, msg_ricevuto.dato, NULL);

            // Simulo una ricerca per un informazione da fornire al client
            struct timespec attesa = {0, 500000000L}; 
            nanosleep(&attesa, NULL);

            char risposta[] = "INFO_TROVATA";
            // resetto la pipe in caso si fosse sporcata
            pipe_rotta = 0;

            if (send(sock, risposta, sizeof(risposta), 0) < 0 || pipe_rotta){
                printf("\n[SEGNALE] Catturato SIGPIPE! Il client (ID: %d) è caduto o si è disconnesso bruscamente.\n", id_corrente);
                scrivi_log(id_corrente, 0, "DISCONNECT");
            }
        }
    } else { perror("Errore durante la read dal socket"); }
    close(sock);
    decrementa_thread();
    pthread_exit(NULL);
}


int main(){
struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = gestore_sigalarm;
    sigaction(SIGALRM, &sa, NULL);

    sa.sa_handler = gestore_sigpipe;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = gestore_sigint;
    sigaction(SIGINT, &sa, NULL);

    // avvio timer per il controllo della dimensione del file
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000; 
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000;  
    setitimer(ITIMER_REAL, &timer, NULL);


    int new_socket;
    // struct che contiene info su indirizzo di rete (IP, porta)
    struct sockaddr_in address; 
    int opt = 1;        // mi serve per il riutilizzo immediato di una porta 
    // dim in byte della struct address, mi servirà per accept e ...
    socklen_t addrlen = sizeof(address);

    // var per gestire i nomi degli archivi in modo sicuro
    int numero_archivio = 1;
    puts("Avvio del coordinatore...");

    // creazione del socket TCP, AF_INET (IpV4), SOCK_STREAM (TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Errore: Creazione socket fallita");
        exit(EXIT_FAILURE);
    }

    // Mi permette di riattivare il server anche se la porta era appena stata usata,
    // tramite le macro SOL_SOCKET e SO_REUSEADDR, lavorano insieme, la prima indica che si vuole
    // modificare una regola generica ad un livello del iso la seconda per il riutilizzo
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Errore: setsockopt fallita");
        exit(EXIT_FAILURE);
    }

    // configuro la struct address (indirizzo IP, accetta connessioni da più interfacce rete, conversione della porta nel formato di rete)
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind per associare IP e porta al socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Errore: Bind fallita");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Metto il server in ascolto
    if (listen(server_fd, CONNESSIONI_IN_CODA) < 0) {
        perror("Errore: Listen fallita");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("In ascolto sulla porta %d...\n", PORT);

    // imposto un ciclo infinito per accettare più connessioni
    while (server_attivo) {
        // bloccante, fino a che non arriva un client
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    
        if (new_socket < 0) {
            /* errno è la var globale che contiene l'ultimo errore, 
            se è uguale a EINTR(Error: interrupted syscall), ma qualcosa ha interrotto la accept, se è cosi
            devo controllare subito i segnali critici "SIGALARM" e "SIGINT"*/
            if (errno == EINTR) {
                // controllo SIGINT
                if (!server_attivo) { break; }
                //controllo SIGALARM
                if (allarme_scattato) {
                    allarme_scattato = 0;
                    
                    struct stat info_file;
                    pthread_mutex_lock(&mutex_per_log);
                    
                    if (stat("log.txt", &info_file) == 0) {
                        if (info_file.st_size > DIMENSIONE_MAX_LOG) {
                            char nuovo_nome_log[64];
                            sprintf(nuovo_nome_log, "log_archivio_%d.txt", numero_archivio++);
                            rename("log.txt", nuovo_nome_log);
                            printf("\n[ALLARME] Il log ha superato i %d byte. File archiviato in %s!\n", DIMENSIONE_MAX_LOG, nuovo_nome_log);
                        }
                    }

                    pthread_mutex_unlock(&mutex_per_log);
                    timer.it_value.tv_sec = 0;
                    timer.it_value.tv_usec = 100000; 
                    timer.it_interval.tv_sec = 0;
                    timer.it_interval.tv_usec = 100000;   
                    setitimer(ITIMER_REAL, &timer, NULL);
                } continue; // continuo ad acoltare

            } continue; // ignoro un altro tipo di errore
        }

        puts("-> Nuovo Client connesso con successo!");

        // devo allocare memoria per il nuovo socket cosi che ogni nuovo socket abbia il suo spazio
        // altrimenti passare direttamente &new_socket al thread potrebbe creare race conditiion
        int *new_sock_ptr = malloc(sizeof(int));
        *new_sock_ptr = new_socket;
        pthread_t thread_id;
        // creo il thread e passo il puntatore al docket
        if (pthread_create(&thread_id, NULL, gestisci_client, (void*)new_sock_ptr) < 0) {
        free(new_sock_ptr);
        close(new_socket);
        } else {
            // uso detach altrimenti il ciclo si blocca per aspettare la fine del thread, ma avendo un ciclo infinito
            // necessito che vada avnti per operare correttamente con altri client
            pthread_detach(thread_id);
        }
    }

    puts("[SERVER] Smetto di accettare nuove connessioni.");

    close(server_fd);

    while(1) {
        pthread_mutex_lock(&mutex_contatore);
        int thread_rimanenti = thread_attivi;
        pthread_mutex_unlock(&mutex_contatore);

        if (thread_rimanenti == 0) { break; }
        printf("[SERVER] Attendo che %d thread finiscano di scrivere...\n", thread_rimanenti);
        sleep(1);
    }

    puts("[SERVER] Tutti i dati sono stati scritti.\n Spegnimento completato.");
    return 0;
}

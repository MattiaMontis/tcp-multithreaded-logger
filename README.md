# tcp-multithreaded-logger
A POSIX-compliant multi-threaded TCP Client-Server application in C for concurrent data logging, featuring system-level file locking and async signal handling.


# 📡 TCP Multi-Threaded Logger (Coordinatore e Produttori)

Un sistema Client-Server TCP scritto in C (standard POSIX) per l'aggregazione sicura e concorrente di dati. Il progetto implementa un server multithread capace di gestire molteplici connessioni simultanee, garantendo l'integrità della scrittura su file tramite un sistema di sincronizzazione a doppio strato (Mutex + File Locking di Sistema).

## 🚀 Funzionalità Principali

- **Architettura Concorrente:** Il server (Coordinatore) assegna un thread `detached` a ogni client (Produttore) connesso, garantendo alta scalabilità e assenza di colli di bottiglia.
- **Sincronizzazione a Doppio Strato:** La scrittura concorrente sul file di log è protetta sia a livello di thread (tramite `pthread_mutex`) sia a livello di processi esterni (tramite record lock POSIX con `fcntl`).
- **Comunicazione Binaria:** Scambio dati ottimizzato trasmettendo direttamente struct C sul socket, azzerando l'overhead del parsing di stringhe.
- **Log Rotation Asincrona:** Utilizzo di un timer asincrono (`setitimer`) e del segnale `SIGALRM` per monitorare la dimensione del file e archiviarlo dinamicamente, sfruttando le interruzioni `EINTR` sulla syscall `accept`.
- **Resilienza e Fault Tolerance:** Intercettazione dei crash improvvisi dei client tramite il segnale `SIGPIPE`, garantendo la stabilità del server e la registrazione dell'evento ("DISCONNECT").
- **Graceful Shutdown:** Spegnimento controllato del server alla ricezione di `SIGINT` (Ctrl+C), con attesa passiva della terminazione di tutte le scritture pendenti.

## 📁 Struttura del Progetto

- `common.h` : Definisce i parametri di rete (Porta 8080) e il payload binario (`struct Messaggio`).
- `server.c` : Il demone Coordinatore (socket TCP in ascolto, multithreading, log management).
- `client.c` : Il Produttore (connessione TCP, invio dati, simulazione stocastica di crash).

## 🛠️ Highlights Tecnici

Questo progetto affronta e risolve alcune classiche sfide della programmazione di sistema Linux:
1. **Thread Local Storage (TLS):** Utilizzo della keyword `__thread` per allocare variabili indipendenti nello stack di ogni thread (es. ID del client corrente), eliminando le Race Condition sulla memoria globale.
2. **Il Paradosso di `fcntl`:** Poiché `fcntl` applica lock basati sul PID (ignorando i singoli thread di uno stesso processo), il progetto implementa un `pthread_mutex` a monte per serializzare i thread, delegando a `fcntl` la sola protezione contro processi esterni.
3. **Simulazione TCP RST:** Il client integra una routine di test che, nel 30% dei casi, altera l'opzione `SO_LINGER` con timeout a 0 prima di chiudere il socket. Questo invia un pacchetto *TCP Reset* anziché un *FIN*, permettendo di testare la corretta generazione del `SIGPIPE` lato server.
4. **Fast Restart:** Implementazione di `SO_REUSEADDR` per bypassare lo stato `TIME_WAIT` della porta TCP, permettendo riavvii immediati del server.

## 💻 Compilazione e Avvio

Il progetto richiede un ambiente Linux/POSIX e il compilatore GCC.

:

💡 Testa subito nel browser: Non hai un ambiente Linux sottomano? Clicca sul pulsante verde Code in alto a destra, seleziona Codespaces e avvia una macchina virtuale gratuita. Potrai incollare i comandi di compilazione nel terminale integrato e testare client e server in pochi secondi.

**1. Compilazione:**
```bash
# Compila il server abilitando la libreria pthread
gcc server.c -o server -pthread -Wall -Wextra

# Compila il client
gcc client.c -o client -Wall -Wextra

#Esecuzione del Server:
./server

#Esecuzione del Client (in un altro terminale):
./client <ID_MITTENTE> <DATO_NUMERICO>
# Esempio: ./client 104 2500

#### Nota: Eseguendo più volte il client, c'è un 30% di probabilità di innescare la simulazione del crash di rete (DISCONNECT).

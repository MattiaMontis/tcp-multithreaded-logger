all: server client

server: server.c common.h
	gcc -Wall -Wextra -g -pthread server.c -o server

client: client.c common.h
	gcc -Wall -Wextra -g -pthread client.c -o client

scripts:
	chmod +x test/*.sh

test_concorrenza: all scripts
	cd test && ./test_concorrenza.sh

test_stress: all scripts
	cd test && ./stress_test.sh

test_valgrind: all
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./server

pulisci:
	rm -f server client log.txt log_archivio_*.txt

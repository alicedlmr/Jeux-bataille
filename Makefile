
CC = gcc
CFLAGS = -Wall -Ibase  


TARGETS = serveur_tcp client_tcp serveur_udp client_udp


all: $(TARGETS)


serveur_tcp: src/serveur_tcp.c
	$(CC) $(CFLAGS) -o serveur_tcp src/serveur_tcp.c

client_tcp: src/client_tcp.c
	$(CC) $(CFLAGS) -o client_tcp src/client_tcp.c

# Regras para UDP
serveur_udp: src/serveur_udp.c
	$(CC) $(CFLAGS) -o serveur_udp src/serveur_udp.c

client_udp: src/client_udp.c
	$(CC) $(CFLAGS) -o client_udp src/client_udp.c


clean:
	rm -f $(TARGETS)

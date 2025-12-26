/*
 * client_udp.c
 * Cliente Bataille via UDP
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "../base/jeux.h" 

void error(const char *msg) { perror(msg); exit(0); }

int main(int argc, char *argv[]) {
    int sockfd, portno;
    unsigned int length;
    struct sockaddr_in serv_addr, from_addr;
    struct hostent *server;
    GamePacket paquet;

    if (argc < 3) { fprintf(stderr,"usage %s hostname port\n", argv[0]); exit(0); }
    
    portno = atoi(argv[2]);
    
    // 1. Socket DGRAM (UDP)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERROR socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) { fprintf(stderr,"ERROR host\n"); exit(0); }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    length = sizeof(struct sockaddr_in);

    // 2. HANDSHAKE INICIAL (Necessário para UDP Server conhecer o cliente)
    printf("[CLIENT UDP] Envoi du signal de presence...\n");
    bzero(&paquet, sizeof(paquet));
    paquet.type = TYPE_ATTENTE; 
    strcpy(paquet.texteInfo, "LOGIN");
    
    // Envia pacote inicial para o servidor capturar IP/Porta
    if (sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length) < 0)
        error("sendto");

    printf("[CLIENT] Attente du demarrage...\n");

    // Loop Principal (Igual ao TCP, mas com recvfrom/sendto)
    while(1) {
        bzero(&paquet, sizeof(paquet));
        
        // Bloqueia esperando mensagem do servidor
        if (recvfrom(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&from_addr, &length) < 0)
            error("recvfrom");

        // Lógica de UI idêntica ao client_tcp.c
        switch(paquet.type) {
            case TYPE_VOTE:
                printf("\n--- VOTE ---\n%s\n", paquet.texteInfo);
                int choixTheme;
                printf("Choix (1-2): "); scanf("%d", &choixTheme);
                
                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = choixTheme;
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length);
                break;
            
            case TYPE_TON_TOUR:
                printf("\n--- A VOUS ---\nCarte: %s\n", paquet.carteInfo.nom);
                printf("1. Crit1: %d\n2. Crit2: %d\n", paquet.carteInfo.critere1, paquet.carteInfo.critere2);
                
                int c;
                printf("Choix Critere (1-2): "); scanf("%d", &c);
                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = c;
                
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length);
                break;

            case TYPE_ADVERSAIRE:
            case TYPE_ATTENTE:
                printf("[INFO] %s\n", paquet.texteInfo);
                break;

            case TYPE_RESULTAT:
                printf("[RESULTAT] %s\n", paquet.texteInfo);
                break;

            case TYPE_FIN:
                printf("FIN: %s\n", paquet.texteInfo);
                close(sockfd);
                return 0;
        }
    }
}
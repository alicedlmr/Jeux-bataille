/*
 * client_udp.c
 * Client Bataille via UDP (4 Critères)
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
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERREUR socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) { fprintf(stderr,"ERREUR hote\n"); exit(0); }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    length = sizeof(struct sockaddr_in);

    // HANDSHAKE INITIAL
    printf("[CLIENT UDP] Signal de presence...\n");
    bzero(&paquet, sizeof(paquet));
    paquet.type = TYPE_ATTENTE; 
    strcpy(paquet.texteInfo, "LOGIN");
    
    if (sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length) < 0)
        error("sendto");

    printf("[CLIENT] En attente...\n");

    while(1) {
        bzero(&paquet, sizeof(paquet));
        
        if (recvfrom(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&from_addr, &length) < 0)
            error("recvfrom");

        switch(paquet.type) {

            // --- REDIRECTION (MULTI-CLIENTS) ---
            case TYPE_REDIRECT:
                printf("[CLIENT] Redirection vers une salle de jeu.\n");
                int new_port = atoi(paquet.texteInfo);
                serv_addr.sin_port = htons(new_port);
                printf("[CLIENT] Changement vers port %d...\n", new_port);
                
                // Confirmation au nouveau serveur
                paquet.type = TYPE_ATTENTE;
                strcpy(paquet.texteInfo, "PRET");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length);
                break;

            case TYPE_TON_TOUR:
                printf("\n##################################\n");
                printf("###      A VOUS DE JOUER !     ###\n");
                printf("##################################\n");
                printf("MA CARTE : %s\n", paquet.carteInfo.nom);
                printf("----------------------------------\n");
                printf("[1] Vitesse Max   : %d km/h\n", paquet.carteInfo.critere1);
                printf("[2] Puissance     : %d ch\n", paquet.carteInfo.critere2);
                printf("[3] Cylindree     : %d cm3\n", paquet.carteInfo.critere3);
                printf("[4] Regime Moteur : %d tr/min\n", paquet.carteInfo.critere4);
                printf("----------------------------------\n");
                printf("%s\n", paquet.texteInfo);
                
                int c;
                do {
                    printf("Votre choix (1-4) : ");
                    if(scanf("%d", &c) != 1) while(getchar() != '\n');
                } while(c < 1 || c > 4);

                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = c;
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&serv_addr, length);
                break;

case TYPE_ADVERSAIRE:
                printf("\n----------------------------------\n");
                printf("   EN ATTENTE DE L'ADVERSAIRE...  \n");
                printf("----------------------------------\n");
                printf("MA CARTE : %s\n", paquet.carteInfo.nom);
                printf(" [1] Vitesse   : %d km/h\n", paquet.carteInfo.critere1);
                printf(" [2] Puissance : %d ch\n", paquet.carteInfo.critere2);
                printf(" [3] Cylindree : %d cm3\n", paquet.carteInfo.critere3);
                printf(" [4] Regime    : %d tr/min\n", paquet.carteInfo.critere4);
                printf("----------------------------------\n");
                printf("INFO : %s\n", paquet.texteInfo);
                break;

            case TYPE_ATTENTE:
                printf("[INFO] %s\n", paquet.texteInfo);
                break;

            case TYPE_RESULTAT:
                printf("\n>>> RESULTAT : %s <<<\n", paquet.texteInfo);
                break;

            case TYPE_FIN:
                printf("\n=============================\n");
                printf("FIN : %s\n", paquet.texteInfo);
                printf("=============================\n");
                close(sockfd);
                return 0;
        }
    }
    return 0;
}

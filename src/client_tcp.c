/*
 * client_tcp.c
 * Client simplifié pour Bataille Automatique
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
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    GamePacket paquet;

    if (argc < 3) { fprintf(stderr,"usage %s hostname port\n", argv[0]); exit(0); }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) { fprintf(stderr,"ERROR host\n"); exit(0); }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    printf("[CLIENT] Connecte. Le jeu va commencer...\n");

    while(1) {
        bzero((char *) &paquet, sizeof(paquet));
        n = read(sockfd, &paquet, sizeof(paquet));
        if (n <= 0) break;

        switch(paquet.type) {

            case TYPE_VOTE:
                printf("\n#################################\n");
                printf("###   SELECTION DU THEME      ###\n");
                printf("#################################\n");
                printf("%s\n", paquet.texteInfo);
                
                int choixTheme;
                do {
                    printf("Votre vote (1 ou 2) : ");
                    scanf("%d", &choixTheme);
                    while(getchar() != '\n'); // Vidage buffer
                } while (choixTheme < 1 || choixTheme > 2);

                // Réponse
                bzero((char *) &paquet, sizeof(paquet));
                paquet.type = TYPE_CHOIX; // On utilise le type réponse standard
                paquet.choixCritere = choixTheme;
                write(sockfd, &paquet, sizeof(paquet));
                
                printf("Vote envoye. Attente du resultat...\n");
                break;
            
            case TYPE_TON_TOUR:
                printf("\n--- C'EST A VOUS ! ---\n");
                printf("Votre carte : %s\n", paquet.carteInfo.nom);
                printf("1. Vitesse   : %d\n", paquet.carteInfo.critere1);
                printf("2. Puissance : %d\n", paquet.carteInfo.critere2);
                printf("----------------------\n");
                printf("%s\n", paquet.texteInfo);
                
                int choixCrit;
                do {
                    printf("Choisissez votre atout (1 ou 2) : ");
                    scanf("%d", &choixCrit);
                    // Vidage buffer clavier au cas où
                    while(getchar() != '\n'); 
                } while(choixCrit < 1 || choixCrit > 2);

                // Réponse
                bzero((char *) &paquet, sizeof(paquet));
                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = choixCrit;
                write(sockfd, &paquet, sizeof(paquet));
                printf("Atout envoye...\n");
                break;

            case TYPE_ADVERSAIRE:
                printf("\n[ATTENTE] %s\n", paquet.texteInfo);
                break;

            case TYPE_RESULTAT:
                printf("\n>>> RESULTAT DU TOUR : %s <<<\n", paquet.texteInfo);
                break;

            case TYPE_FIN:
                printf("\n=============================\n");
                printf("FIN : %s\n", paquet.texteInfo);
                printf("=============================\n");
                close(sockfd);
                return 0;
        }
    }
    close(sockfd);
    return 0;
}
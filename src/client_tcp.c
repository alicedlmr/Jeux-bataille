/* src/client_tcp.c */
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

    printf("[CLIENT] Connecte au serveur de jeu.\n");

    while(1) {
        bzero((char *) &paquet, sizeof(paquet));
        n = read(sockfd, &paquet, sizeof(paquet));
        if (n <= 0) break;

        switch(paquet.type) {
            
            case TYPE_TON_TOUR:
                printf("\n##################################\n");
                printf("###      C'EST A VOUS !        ###\n");
                printf("##################################\n");
                printf("MA CARTE : %s\n", paquet.carteInfo.nom);
                printf("----------------------------------\n");
                printf("[1] Vitesse Max   : %d km/h\n", paquet.carteInfo.critere1);
                printf("[2] Puissance     : %d ch\n", paquet.carteInfo.critere2);
                printf("[3] Cylindree     : %d cm3\n", paquet.carteInfo.critere3);
                printf("[4] Regime Moteur : %d tr/min\n", paquet.carteInfo.critere4);
                printf("----------------------------------\n");
                printf("%s\n", paquet.texteInfo);
                
                int choixCrit;
                do {
                    printf("Votre choix (1-4) : ");
                    if(scanf("%d", &choixCrit) != 1) while(getchar() != '\n'); 
                } while(choixCrit < 1 || choixCrit > 4);

                // Envoi de la réponse
                bzero((char *) &paquet, sizeof(paquet));
                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = choixCrit;
                write(sockfd, &paquet, sizeof(paquet));
                printf("... Choix envoye ...\n");
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

            case TYPE_RESULTAT:
                printf("\n>>> RESULTAT MANCHE : %s <<<\n", paquet.texteInfo);
                break;

            case TYPE_FIN:
                printf("\n=============================\n");
                printf("FIN DE PARTIE : %s\n", paquet.texteInfo);
                printf("=============================\n");
                close(sockfd);
                return 0;
        }
    }
    close(sockfd);
    return 0;
}

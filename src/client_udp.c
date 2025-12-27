/*
 * client_udp.c
 * Client Bataille via UDP
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

    if (argc < 3) { 
        fprintf(stderr,"usage %s nom_hote port\n", argv[0]); 
        exit(0); 
    }
    
    portno = atoi(argv[2]);
    
    // 1. Socket DGRAM (UDP)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERREUR socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) { 
        fprintf(stderr,"ERREUR hote\n"); 
        exit(0); 
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
          (char *)&serv_addr.sin_addr.s_addr, 
          server->h_length);
    serv_addr.sin_port = htons(portno);
    length = sizeof(struct sockaddr_in);

    // 2. HANDSHAKE INITIAL (nécessaire pour que le serveur UDP connaisse le client)
    printf("[CLIENT UDP] Envoi du signal de presence...\n");
    bzero(&paquet, sizeof(paquet));
    paquet.type = TYPE_ATTENTE; 
    strcpy(paquet.texteInfo, "LOGIN");
    
    // Envoie le paquet initial pour que le serveur capture IP/Port
    if (sendto(sockfd, &paquet, sizeof(paquet), 0, 
               (struct sockaddr *)&serv_addr, length) < 0)
        error("sendto");

    printf("[CLIENT] Attente du demarrage...\n");

    // Boucle principale
    while(1) {
        bzero(&paquet, sizeof(paquet));
        
        // Bloque en attente d'un message
        if (recvfrom(sockfd, &paquet, sizeof(paquet), 0, 
                     (struct sockaddr *)&from_addr, &length) < 0)
            error("recvfrom");

        switch(paquet.type) {

            // --- NOUVEAU : GESTION DE LA REDIRECTION ---
            case TYPE_REDIRECT:
                printf("[CLIENT] Le serveur a change de salle de jeu.\n");
                int new_port = atoi(paquet.texteInfo);
                
                // Met à jour l'adresse du serveur avec le nouveau port
                serv_addr.sin_port = htons(new_port);
                printf("[CLIENT] Changement vers le port %d...\n", new_port);
                
                // Envoie un "bonjour" sur le nouveau port pour confirmer la presence
                paquet.type = TYPE_ATTENTE;
                strcpy(paquet.texteInfo, "PRET");
                sendto(sockfd, &paquet, sizeof(paquet), 0, 
                       (struct sockaddr *)&serv_addr, length);
                break;
            // --------------------------------------------

            case TYPE_VOTE:
                printf("\n--- VOTE ---\n%s\n", paquet.texteInfo);
                int choixTheme;
                do {
                    printf("Choix (1-2) : "); 
                    if(scanf("%d", &choixTheme) != 1)
                        while(getchar() != '\n');
                } while(choixTheme < 1 || choixTheme > 2);
                
                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = choixTheme;
                sendto(sockfd, &paquet, sizeof(paquet), 0, 
                       (struct sockaddr *)&serv_addr, length);
                break;
            
            case TYPE_TON_TOUR:
                printf("\n--- A VOUS DE JOUER ---\nCarte : %s\n", 
                       paquet.carteInfo.nom);
                printf("1. Critere 1 : %d\n2. Critere 2 : %d\n", 
                       paquet.carteInfo.critere1, 
                       paquet.carteInfo.critere2);
                
                int c;
                do {
                    printf("Choix du critere (1-2) : ");
                    if(scanf("%d", &c) != 1)
                        while(getchar() != '\n');
                } while(c < 1 || c > 2);

                paquet.type = TYPE_CHOIX;
                paquet.choixCritere = c;
                sendto(sockfd, &paquet, sizeof(paquet), 0, 
                       (struct sockaddr *)&serv_addr, length);
                break;

            case TYPE_ADVERSAIRE:
            case TYPE_ATTENTE:
                printf("[INFO] %s\n", paquet.texteInfo);
                break;

            case TYPE_RESULTAT:
                printf("[RESULTAT] %s\n", paquet.texteInfo);
                break;

            case TYPE_FIN:
                printf("FIN : %s\n", paquet.texteInfo);
                close(sockfd);
                return 0;
        }
    }

    close(sockfd);
    return 0;
}


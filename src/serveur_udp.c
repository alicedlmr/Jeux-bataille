/*
 * server_udp.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 
#include "../base/jeux.h" 


#define TOTAL_CARTES_JEU 10
#define MAIN_JOUEUR 5

Carte master_deck[TOTAL_CARTES_JEU];
Carte deck_j1[TOTAL_CARTES_JEU]; 
Carte deck_j2[TOTAL_CARTES_JEU];
int nb_cartes_j1 = 0;
int nb_cartes_j2 = 0;

Carte temp_j1[TOTAL_CARTES_JEU];
Carte temp_j2[TOTAL_CARTES_JEU];
int nb_temp_j1 = 0;
int nb_temp_j2 = 0;

// --- FUNÇÕES UTILITÁRIAS (Replicadas do TCP) ---
void error(const char *msg) { perror(msg); exit(1); }

void melanger_deck(Carte *deck, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Carte temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void recycler_deck(Carte *main_deck, int *nb_main, Carte *temp_deck, int *nb_temp) {
    printf("[SERVEUR] Recyclage du deck...\n");
    for(int i=0; i < *nb_temp; i++) {
        main_deck[i] = temp_deck[i];
    }
    *nb_main = *nb_temp;
    *nb_temp = 0;
    melanger_deck(main_deck, *nb_main);
}

void ajouter_carte(Carte *deck, int *nb_cartes, Carte c) {
    if (*nb_cartes < TOTAL_CARTES_JEU) {
        deck[*nb_cartes] = c;
        (*nb_cartes)++;
    }
}

void supprimer_top_carte(Carte *deck, int *nb_cartes) {
    for (int i = 0; i < *nb_cartes - 1; i++) {
        deck[i] = deck[i+1];
    }
    (*nb_cartes)--;
}

void set_carte(Carte *c, char *nom, int crit1, int crit2) {
    strcpy(c->nom, nom);
    c->critere1 = crit1;
    c->critere2 = crit2;
}

void charger_voitures() {
    printf("[DEBUG] Chargement VOITURES\n");
    set_carte(&master_deck[0], "Ferrari F50", 325, 520);
    set_carte(&master_deck[1], "Porsche 911", 300, 450);
    set_carte(&master_deck[2], "Fiat Panda", 140, 60);
    set_carte(&master_deck[3], "Bugatti Veyron", 400, 1001);
    set_carte(&master_deck[4], "Renault Twingo", 150, 55);
    set_carte(&master_deck[5], "Audi R8", 315, 540);
    set_carte(&master_deck[6], "Citroen 2CV", 90, 29);
    set_carte(&master_deck[7], "Hummer H2", 160, 393);
    set_carte(&master_deck[8], "Tesla Model S", 250, 600);
    set_carte(&master_deck[9], "Peugeot 208", 190, 110);
}

void charger_animaux() {
    printf("[DEBUG] Chargement ANIMAUX\n");
    set_carte(&master_deck[0], "Elephant", 6000, 70);
    set_carte(&master_deck[1], "Lion", 190, 14);
    set_carte(&master_deck[2], "Tortue Geante", 250, 150);
    set_carte(&master_deck[3], "Aigle", 5, 20);
    set_carte(&master_deck[4], "Baleine Bleue", 150000, 90);
    set_carte(&master_deck[5], "Chien", 30, 13);
    set_carte(&master_deck[6], "Hamster", 1, 3);
    set_carte(&master_deck[7], "Crocodile", 500, 70);
    set_carte(&master_deck[8], "Cheval", 400, 25);
    set_carte(&master_deck[9], "Humain", 80, 85);
}

void init_jeu(int choix_theme) {
    if (choix_theme == 1) charger_voitures();
    else charger_animaux();

    melanger_deck(master_deck, TOTAL_CARTES_JEU);

    nb_cartes_j1 = 0; nb_cartes_j2 = 0;
    nb_temp_j1 = 0; nb_temp_j2 = 0;

    for(int i=0; i < MAIN_JOUEUR; i++) {
        ajouter_carte(deck_j1, &nb_cartes_j1, master_deck[i]);
        ajouter_carte(deck_j2, &nb_cartes_j2, master_deck[i + MAIN_JOUEUR]);
    }
}

// --- MAIN UDP ---
int main(int argc, char *argv[]) {
    int sockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    
    struct sockaddr_in addr_j1, addr_j2;
    int j1_connected = 0, j2_connected = 0;

    srand(time(NULL)); 

    if (argc < 2) { fprintf(stderr,"Port manquant\n"); exit(1); }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    printf("[SERVEUR UDP] Attente sur port %d...\n", portno);
    clilen = sizeof(cli_addr);
    GamePacket buffer;

    // --- 1. CONEXÃO (HANDSHAKE) ---
    while (!j1_connected || !j2_connected) {
        bzero(&buffer, sizeof(buffer));
        recvfrom(sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &clilen);

        if (!j1_connected) {
            addr_j1 = cli_addr;
            j1_connected = 1;
            printf("J1 connecte.\n");
            strcpy(buffer.texteInfo, "Bienvenue J1. Attente de J2...");
            buffer.type = TYPE_ATTENTE;
            sendto(sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*)&addr_j1, clilen);
        } 
        else if (!j2_connected) {
            if (cli_addr.sin_addr.s_addr != addr_j1.sin_addr.s_addr || cli_addr.sin_port != addr_j1.sin_port) {
                addr_j2 = cli_addr;
                j2_connected = 1;
                printf("J2 connecte.\n");
            }
        }
    }

    // --- 2. VOTAÇÃO ---
    GamePacket p_vote;
    bzero((char *) &p_vote, sizeof(p_vote));
    p_vote.type = TYPE_VOTE;
    strcpy(p_vote.texteInfo, "Votez theme: 1. Voitures / 2. Animaux");

    sendto(sockfd, &p_vote, sizeof(p_vote), 0, (struct sockaddr*)&addr_j1, clilen);
    sendto(sockfd, &p_vote, sizeof(p_vote), 0, (struct sockaddr*)&addr_j2, clilen);

    int vote1 = 1, vote2 = 1;
    for(int i=0; i<2; i++) {
        recvfrom(sockfd, &p_vote, sizeof(p_vote), 0, (struct sockaddr *)&cli_addr, &clilen);
        if (cli_addr.sin_port == addr_j1.sin_port) vote1 = p_vote.choixCritere;
        else vote2 = p_vote.choixCritere;
    }

    int theme_final = (vote1 == vote2) ? vote1 : (rand() % 2) + 1;
    printf("THEME CHOISI: %d\n", theme_final);
    
    // *** IMPORTANTE: INICIALIZAR AS CARTAS AQUI ***
    init_jeu(theme_final); 

    // --- 3. LOOP DO JOGO ---
    int tour_j1 = 1; 
    int partie_active = 1;
    GamePacket paquet;

    while (partie_active) {
        // Verifica Vitoria/Derrota
        if (nb_cartes_j1 == 0) {
            if (nb_temp_j1 > 0) recycler_deck(deck_j1, &nb_cartes_j1, temp_j1, &nb_temp_j1);
            else { 
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "J2 GAGNE!");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)&addr_j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)&addr_j2, clilen);
                break;
            }
        }
        if (nb_cartes_j2 == 0) {
            if (nb_temp_j2 > 0) recycler_deck(deck_j2, &nb_cartes_j2, temp_j2, &nb_temp_j2);
            else { 
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "J1 GAGNE!");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)&addr_j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)&addr_j2, clilen);
                break;
            }
        }

        struct sockaddr_in *addr_actif = (tour_j1) ? &addr_j1 : &addr_j2;
        struct sockaddr_in *addr_passif = (tour_j1) ? &addr_j2 : &addr_j1;
        Carte *deck_actif = (tour_j1) ? deck_j1 : deck_j2;
        Carte *deck_passif = (tour_j1) ? deck_j2 : deck_j1;
        Carte *temp_actif = (tour_j1) ? temp_j1 : temp_j2;
        Carte *temp_passif = (tour_j1) ? temp_j2 : temp_j1;
        int *nb_temp_actif = (tour_j1) ? &nb_temp_j1 : &nb_temp_j2;
        int *nb_temp_passif = (tour_j1) ? &nb_temp_j2 : &nb_temp_j1;

        // *** IMPORTANTE: PEGAR OS DADOS DA CARTA REAL ***
        Carte c1 = deck_actif[0];
        Carte c2 = deck_passif[0];

        bzero(&paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1; // <--- O SEU ERRO ESTAVA AQUI (FALTAVA ISSO)
        sprintf(paquet.texteInfo, "A vous de jouer (Cartes: %d)", (tour_j1 ? nb_cartes_j1 : nb_cartes_j2));
        
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)addr_actif, clilen);

        paquet.type = TYPE_ADVERSAIRE;
        strcpy(paquet.texteInfo, "Adversaire reflechit...");
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)addr_passif, clilen);

        // Receber escolha
        do {
            recvfrom(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&cli_addr, &clilen);
        } while (cli_addr.sin_port != addr_actif->sin_port);

        int critere = paquet.choixCritere;
        int val1 = (critere == 1) ? c1.critere1 : c1.critere2;
        int val2 = (critere == 1) ? c2.critere1 : c2.critere2;
        int j_actif_gagne = (val1 >= val2);

        // Resolver
        if (j_actif_gagne) {
            sprintf(paquet.texteInfo, "GAGNE! %d vs %d (%s)", val1, val2, c2.nom);
            ajouter_carte(temp_actif, nb_temp_actif, c1);
            ajouter_carte(temp_actif, nb_temp_actif, c2);
        } else {
            sprintf(paquet.texteInfo, "PERDU... %d vs %d (%s)", val1, val2, c2.nom);
            ajouter_carte(temp_passif, nb_temp_passif, c1);
            ajouter_carte(temp_passif, nb_temp_passif, c2);
            tour_j1 = !tour_j1;
        }

        supprimer_top_carte(deck_j1, &nb_cartes_j1);
        supprimer_top_carte(deck_j2, &nb_cartes_j2);

        // Enviar Resultados
        paquet.type = TYPE_RESULTAT; // O texto já está no paquet.texteInfo
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)addr_actif, clilen);
        
        if (j_actif_gagne) strcpy(paquet.texteInfo, "Adversaire a gagne la manche.");
        else strcpy(paquet.texteInfo, "VOUS AVEZ GAGNE la manche!");
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr*)addr_passif, clilen);
    }

    close(sockfd);
    return 0;
}

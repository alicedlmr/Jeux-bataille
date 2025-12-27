/*
 * server_udp.c – Version MULTI-CLIENTS
 * Utilise fork() et redirection de port
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
#include <signal.h>
#include <sys/wait.h>
#include "../base/jeux.h" 

// --- CONSTANTES ET GLOBALES ---
#define TOTAL_CARTES_JEU 10
#define MAIN_JOUEUR 5

// Structures de données globales (définitions uniquement)
// Les variables d’état du jeu sont locales ou isolées par processus fork
Carte master_deck[TOTAL_CARTES_JEU];

// --- FONCTIONS UTILITAIRES ---
void error(const char *msg) { perror(msg); exit(1); }

// Gestion des processus zombies
void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Mélange un paquet de cartes
void melanger_deck(Carte *deck, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Carte temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// Recycle le paquet temporaire vers le paquet principal
void recycler_deck(Carte *main_deck, int *nb_main, Carte *temp_deck, int *nb_temp) {
    for (int i = 0; i < *nb_temp; i++)
        main_deck[i] = temp_deck[i];
    *nb_main = *nb_temp;
    *nb_temp = 0;
    melanger_deck(main_deck, *nb_main);
}

// Ajoute une carte au bas du paquet
void ajouter_carte(Carte *deck, int *nb_cartes, Carte c) {
    if (*nb_cartes < TOTAL_CARTES_JEU) {
        deck[*nb_cartes] = c;
        (*nb_cartes)++;
    }
}

// Supprime la carte du dessus (index 0)
void supprimer_top_carte(Carte *deck, int *nb_cartes) {
    for (int i = 0; i < *nb_cartes - 1; i++)
        deck[i] = deck[i + 1];
    (*nb_cartes)--;
}

// Initialise une carte
void set_carte(Carte *c, char *nom, int crit1, int crit2) {
    strcpy(c->nom, nom);
    c->critere1 = crit1;
    c->critere2 = crit2;
}

// Chargement des cartes – Thème VOITURES
void charger_voitures() {
    set_carte(&master_deck[0], "Ferrari F50", 325, 520);
    set_carte(&master_deck[1], "Porsche 911", 300, 450);
    set_carte(&master_deck[2], "Fiat Panda", 140, 60);
    set_carte(&master_deck[3], "Bugatti Veyron", 400, 1001);
    set_carte(&master_deck[4], "Renault Twingo", 150, 55);
    set_carte(&master_deck[5], "Audi R8", 315, 540);
    set_carte(&master_deck[6], "Citroën 2CV", 90, 29);
    set_carte(&master_deck[7], "Hummer H2", 160, 393);
    set_carte(&master_deck[8], "Tesla Model S", 250, 600);
    set_carte(&master_deck[9], "Peugeot 208", 190, 110);
}

// Chargement des cartes – Thème ANIMAUX
void charger_animaux() {
    set_carte(&master_deck[0], "Éléphant", 6000, 70);
    set_carte(&master_deck[1], "Lion", 190, 14);
    set_carte(&master_deck[2], "Tortue géante", 250, 150);
    set_carte(&master_deck[3], "Aigle", 5, 20);
    set_carte(&master_deck[4], "Baleine bleue", 150000, 90);
    set_carte(&master_deck[5], "Chien", 30, 13);
    set_carte(&master_deck[6], "Hamster", 1, 3);
    set_carte(&master_deck[7], "Crocodile", 500, 70);
    set_carte(&master_deck[8], "Cheval", 400, 25);
    set_carte(&master_deck[9], "Humain", 80, 85);
}

// Initialisation du jeu avec des paquets locaux (isolation entre processus)
void init_jeu_local(int theme, Carte *d1, int *n1, Carte *d2, int *n2) {
    if (theme == 1) charger_voitures();
    else charger_animaux();

    melanger_deck(master_deck, TOTAL_CARTES_JEU);
    *n1 = 0; *n2 = 0;

    for (int i = 0; i < MAIN_JOUEUR; i++) {
        ajouter_carte(d1, n1, master_deck[i]);
        ajouter_carte(d2, n2, master_deck[i + MAIN_JOUEUR]);
    }
}

// --- LOGIQUE DU JEU (PROCESSUS FILS) ---
void jouer_partie(int sockfd, struct sockaddr_in j1, struct sockaddr_in j2) {
    socklen_t clilen = sizeof(struct sockaddr_in);
    GamePacket paquet, rx_pkt;
    struct sockaddr_in cli_addr;

    // Variables locales du jeu
    Carte deck_j1[TOTAL_CARTES_JEU], deck_j2[TOTAL_CARTES_JEU];
    Carte temp_j1[TOTAL_CARTES_JEU], temp_j2[TOTAL_CARTES_JEU];
    int nb_cartes_j1 = 0, nb_cartes_j2 = 0, nb_temp_j1 = 0, nb_temp_j2 = 0;

    printf("[GAME PID %d] Attente des clients sur le nouveau socket...\n", getpid());

    // Handshake sur le nouveau port
    int acks = 0;
    while (acks < 2) {
        recvfrom(sockfd, &rx_pkt, sizeof(rx_pkt), 0,
                 (struct sockaddr *)&cli_addr, &clilen);
        printf("[GAME PID %d] Client connecté sur la nouvelle salle.\n", getpid());
        acks++;
    }

    // --- PHASE DE VOTE ---
    bzero(&paquet, sizeof(paquet));
    paquet.type = TYPE_VOTE;
    strcpy(paquet.texteInfo, "Votez : 1. Voitures / 2. Animaux");

    sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j1, clilen);
    sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j2, clilen);

    int vote1 = 1, vote2 = 1;
    for (int i = 0; i < 2; i++) {
        recvfrom(sockfd, &rx_pkt, sizeof(rx_pkt), 0,
                 (struct sockaddr *)&cli_addr, &clilen);
        if (cli_addr.sin_port == j1.sin_port)
            vote1 = rx_pkt.choixCritere;
        else
            vote2 = rx_pkt.choixCritere;
    }

    int theme = (vote1 == vote2) ? vote1 : (rand() % 2) + 1;
    init_jeu_local(theme, deck_j1, &nb_cartes_j1, deck_j2, &nb_cartes_j2);
    printf("[GAME PID %d] Thème %d sélectionné. Début de la partie.\n",
           getpid(), theme);

    // --- BOUCLE DE JEU ---
    int tour_j1 = 1;
    int partie_active = 1;

    while (partie_active) {

        // Vérification de fin de partie
        if (nb_cartes_j1 == 0) {
            if (nb_temp_j1 > 0)
                recycler_deck(deck_j1, &nb_cartes_j1, temp_j1, &nb_temp_j1);
            else {
                paquet.type = TYPE_FIN;
                strcpy(paquet.texteInfo, "LE JOUEUR 2 GAGNE !");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j2, clilen);
                break;
            }
        }

        if (nb_cartes_j2 == 0) {
            if (nb_temp_j2 > 0)
                recycler_deck(deck_j2, &nb_cartes_j2, temp_j2, &nb_temp_j2);
            else {
                paquet.type = TYPE_FIN;
                strcpy(paquet.texteInfo, "LE JOUEUR 1 GAGNE !");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j2, clilen);
                break;
            }
        }

        // Sélection des pointeurs actifs / passifs
        struct sockaddr_in *addr_actif = tour_j1 ? &j1 : &j2;
        struct sockaddr_in *addr_passif = tour_j1 ? &j2 : &j1;

        Carte *d_actif = tour_j1 ? deck_j1 : deck_j2;
        Carte *d_passif = tour_j1 ? deck_j2 : deck_j1;

        Carte *t_actif = tour_j1 ? temp_j1 : temp_j2;
        Carte *t_passif = tour_j1 ? temp_j2 : temp_j1;

        int *nt_actif = tour_j1 ? &nb_temp_j1 : &nb_temp_j2;
        int *nt_passif = tour_j1 ? &nb_temp_j2 : &nb_temp_j1;

        Carte c1 = d_actif[0];
        Carte c2 = d_passif[0];

        // Envoi des informations de carte
        bzero(&paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1;
        sprintf(paquet.texteInfo, "À vous de jouer (Cartes restantes : %d)",
                tour_j1 ? nb_cartes_j1 : nb_cartes_j2);
        sendto(sockfd, &paquet, sizeof(paquet), 0,
               (struct sockaddr *)addr_actif, clilen);

        paquet.type = TYPE_ADVERSAIRE;
        strcpy(paquet.texteInfo, "L'adversaire joue...");
        sendto(sockfd, &paquet, sizeof(paquet), 0,
               (struct sockaddr *)addr_passif, clilen);

        // Attente du choix du joueur actif
        do {
            recvfrom(sockfd, &rx_pkt, sizeof(rx_pkt), 0,
                     (struct sockaddr *)&cli_addr, &clilen);
        } while (cli_addr.sin_port != addr_actif->sin_port);

        int crit = rx_pkt.choixCritere;
        int v1 = (crit == 1) ? c1.critere1 : c1.critere2;
        int v2 = (crit == 1) ? c2.critere1 : c2.critere2;
        int win = (v1 >= v2);

        if (win) {
            sprintf(paquet.texteInfo, "GAGNÉ ! %d vs %d (%s)", v1, v2, c2.nom);
            ajouter_carte(t_actif, nt_actif, c1);
            ajouter_carte(t_actif, nt_actif, c2);
        } else {
            sprintf(paquet.texteInfo, "PERDU... %d vs %d (%s)", v1, v2, c2.nom);
            ajouter_carte(t_passif, nt_passif, c1);
            ajouter_carte(t_passif, nt_passif, c2);
            tour_j1 = !tour_j1;
        }

        supprimer_top_carte(deck_j1, &nb_cartes_j1);
        supprimer_top_carte(deck_j2, &nb_cartes_j2);

        paquet.type = TYPE_RESULTAT;
        sendto(sockfd, &paquet, sizeof(paquet), 0,
               (struct sockaddr *)addr_actif, clilen);

        if (win)
            strcpy(paquet.texteInfo, "L'adversaire a perdu la manche.");
        else
            strcpy(paquet.texteInfo, "VOUS AVEZ GAGNÉ la manche !");
        sendto(sockfd, &paquet, sizeof(paquet), 0,
               (struct sockaddr *)addr_passif, clilen);
    }

    printf("[GAME PID %d] Fin de la partie.\n", getpid());
    close(sockfd);
    exit(0); // Termine le processus fils
}

// --- SERVEUR PRINCIPAL (LOBBY) ---
int main(int argc, char *argv[]) {
    int main_sockfd, new_sockfd, portno;
    struct sockaddr_in serv_addr, cli_addr, j1_addr;
    socklen_t clilen = sizeof(cli_addr);
    GamePacket buffer;

    // Gestion des processus zombies
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        error("sigaction");

    if (argc < 2) {
        fprintf(stderr, "Port manquant\n");
        exit(1);
    }

    main_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (main_sockfd < 0) error("ERREUR socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(main_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERREUR bind");

    printf("[SERVER UDP] Lobby ouvert sur le port %d. Attente de joueurs...\n", portno);

    int waiting_j1 = 0;

    while (1) {
        bzero(&buffer, sizeof(buffer));
        if (recvfrom(main_sockfd, &buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&cli_addr, &clilen) < 0)
            continue;

        if (!waiting_j1) {
            j1_addr = cli_addr;
            waiting_j1 = 1;
            printf("[LOBBY] J1 connecté (%d). Attente de J2...\n",
                   ntohs(cli_addr.sin_port));

            buffer.type = TYPE_ATTENTE;
            strcpy(buffer.texteInfo, "Bienvenue. En attente d'un adversaire...");
            sendto(main_sockfd, &buffer, sizeof(buffer), 0,
                   (struct sockaddr *)&j1_addr, clilen);
        } else {
            if (cli_addr.sin_port == j1_addr.sin_port)
                continue;

            printf("[LOBBY] J2 connecté (%d). Création d'une partie...\n",
                   ntohs(cli_addr.sin_port));

            new_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (new_sockfd < 0) error("Socket jeu");

            struct sockaddr_in game_addr;
            bzero(&game_addr, sizeof(game_addr));
            game_addr.sin_family = AF_INET;
            game_addr.sin_addr.s_addr = INADDR_ANY;
            game_addr.sin_port = 0;

            if (bind(new_sockfd, (struct sockaddr *)&game_addr, sizeof(game_addr)) < 0)
                error("Bind jeu");

            socklen_t len = sizeof(game_addr);
            getsockname(new_sockfd, (struct sockaddr *)&game_addr, &len);
            int new_port = ntohs(game_addr.sin_port);

            printf("[LOBBY] Nouvelle salle créée sur le port %d\n", new_port);

            buffer.type = TYPE_REDIRECT;
            sprintf(buffer.texteInfo, "%d", new_port);

            sendto(main_sockfd, &buffer, sizeof(buffer), 0,
                   (struct sockaddr *)&j1_addr, clilen);
            sendto(main_sockfd, &buffer, sizeof(buffer), 0,
                   (struct sockaddr *)&cli_addr, clilen);

            int pid = fork();
            if (pid < 0) error("Fork");

            if (pid == 0) {
                close(main_sockfd);
                jouer_partie(new_sockfd, j1_addr, cli_addr);
            } else {
                close(new_sockfd);
                waiting_j1 = 0;
                printf("[LOBBY] Partie lancée (PID %d). Retour à l'attente...\n", pid);
            }
        }
    }
    return 0;
}


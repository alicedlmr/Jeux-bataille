/*
 * server_udp.c - Version MULTI-CLIENTS
 * Architecture : Redirection de port + Fork
 * Jeu : Bataille de Voitures (4 Critères)
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

// --- CONSTANTES ---
#define TOTAL_CARTES_JEU 20  // 20 cartes équilibrées
#define MAIN_JOUEUR 10       // 10 cartes par joueur

// --- GLOBALES ---
Carte master_deck[TOTAL_CARTES_JEU];

// --- UTILITAIRES ---
void error(const char *msg) { perror(msg); exit(1); }

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void melanger_deck(Carte *deck, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Carte temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void recycler_deck(Carte *main_deck, int *nb_main, Carte *temp_deck, int *nb_temp) {
    for (int i = 0; i < *nb_temp; i++)
        main_deck[i] = temp_deck[i];
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
    for (int i = 0; i < *nb_cartes - 1; i++)
        deck[i] = deck[i + 1];
    (*nb_cartes)--;
}

// Fonction helper pour remplir les 4 critères
void set_c(Carte *c, char *nom, int vit, int ch, int cc, int rpm) {
    strcpy(c->nom, nom);
    c->critere1 = vit; // Vitesse
    c->critere2 = ch;  // Puissance
    c->critere3 = cc;  // Cylindrée
    c->critere4 = rpm; // RPM
}

// --- CHARGEMENT DES DONNÉES (Même que TCP) ---
void charger_voitures() {
    // GROUPE 1 : MUSCLE CARS (Fort en Cylindrée/CC)
    set_c(&master_deck[0], "Dodge Viper GTS", 306, 450, 7990, 5600);
    set_c(&master_deck[1], "Shelby Cobra 427", 260, 425, 7000, 6000);
    set_c(&master_deck[2], "Chevrolet Corvette C6", 300, 430, 6200, 6500);
    set_c(&master_deck[3], "Plymouth Barracuda", 220, 425, 7200, 5000);
    set_c(&master_deck[4], "Ford Mustang GT", 250, 460, 5000, 6500);

    // GROUPE 2 : JDM (Fort en RPM)
    set_c(&master_deck[5], "Honda S2000", 240, 240, 1997, 9000);
    set_c(&master_deck[6], "Mazda RX-7", 250, 280, 1308, 8000);
    set_c(&master_deck[7], "Nissan Skyline R34", 250, 320, 2600, 8000);
    set_c(&master_deck[8], "Toyota Supra MK4", 250, 330, 3000, 6800);
    set_c(&master_deck[9], "Subaru Impreza WRX", 230, 280, 2500, 7000);

    // GROUPE 3 : SUPERCARS (Fort en Vitesse)
    set_c(&master_deck[10], "Ferrari F40", 324, 478, 2900, 7000);
    set_c(&master_deck[11], "Porsche 911 GT3", 310, 500, 4000, 8500);
    set_c(&master_deck[12], "Lamborghini Diablo", 325, 492, 5700, 7000);
    set_c(&master_deck[13], "Audi R8 V10", 316, 525, 5200, 8000);
    set_c(&master_deck[14], "McLaren F1", 386, 627, 6100, 7500);

    // GROUPE 4 : OUTSIDERS
    set_c(&master_deck[15], "Lotus Exige", 240, 220, 1800, 8500);
    set_c(&master_deck[16], "BMW M3 E46", 250, 343, 3200, 7900);
    set_c(&master_deck[17], "Aston Martin DB9", 300, 450, 5900, 6000);
    set_c(&master_deck[18], "TVR Sagaris", 298, 400, 4000, 7000);
    set_c(&master_deck[19], "Fiat 500 Abarth", 210, 160, 1400, 5500);
}

// Initialisation locale (Isolation processus)
void init_jeu_local(Carte *d1, int *n1, Carte *d2, int *n2) {
    charger_voitures();
    melanger_deck(master_deck, TOTAL_CARTES_JEU);
    *n1 = 0; *n2 = 0;

    for (int i = 0; i < MAIN_JOUEUR * 2; i++) {
        if (i % 2 == 0) ajouter_carte(d1, n1, master_deck[i]);
        else ajouter_carte(d2, n2, master_deck[i]);
    }
}

// --- LOGIQUE DU JEU (PROCESSUS FILS) ---
void jouer_partie(int sockfd, struct sockaddr_in j1, struct sockaddr_in j2) {
    socklen_t clilen = sizeof(struct sockaddr_in);
    GamePacket paquet, rx_pkt;
    struct sockaddr_in cli_addr;

    // Variables locales
    Carte deck_j1[TOTAL_CARTES_JEU], deck_j2[TOTAL_CARTES_JEU];
    Carte temp_j1[TOTAL_CARTES_JEU], temp_j2[TOTAL_CARTES_JEU];
    int nb_cartes_j1 = 0, nb_cartes_j2 = 0, nb_temp_j1 = 0, nb_temp_j2 = 0;

    printf("[GAME PID %d] Attente clients sur nouveau port...\n", getpid());

    // Handshake UDP (Confirmation de présence sur le nouveau port)
    int acks = 0;
    while (acks < 2) {
        recvfrom(sockfd, &rx_pkt, sizeof(rx_pkt), 0, (struct sockaddr *)&cli_addr, &clilen);
        printf("[GAME PID %d] Client connecte.\n", getpid());
        acks++;
    }

    // Pas de vote. On initialise directement.
    init_jeu_local(deck_j1, &nb_cartes_j1, deck_j2, &nb_cartes_j2);
    printf("[GAME PID %d] Debut partie (Mode Voitures).\n", getpid());

    int tour_j1 = 1;
    int partie_active = 1;

    while (partie_active) {
        // VICTOIRE / DEFAITE
        if (nb_cartes_j1 == 0) {
            if (nb_temp_j1 > 0) recycler_deck(deck_j1, &nb_cartes_j1, temp_j1, &nb_temp_j1);
            else {
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 2 !");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j2, clilen);
                break;
            }
        }
        if (nb_cartes_j2 == 0) {
            if (nb_temp_j2 > 0) recycler_deck(deck_j2, &nb_cartes_j2, temp_j2, &nb_temp_j2);
            else {
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 1 !");
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j1, clilen);
                sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)&j2, clilen);
                break;
            }
        }

        // Pointeurs dynamiques
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

        // 1. Envoyer infos au joueur actif
        bzero(&paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1;
        sprintf(paquet.texteInfo, "A vous ! (Cartes: %d)", tour_j1 ? nb_cartes_j1 : nb_cartes_j2);
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)addr_actif, clilen);

        // 2. Envoyer attente au passif
        
        paquet.type = TYPE_ADVERSAIRE;
        paquet.carteInfo = c2;
        strcpy(paquet.texteInfo, "L'adversaire choisit une stat...");
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)addr_passif, clilen);

        // 3. Recevoir choix (Filtrage par IP/Port pour sécurité basique)
        do {
            recvfrom(sockfd, &rx_pkt, sizeof(rx_pkt), 0, (struct sockaddr *)&cli_addr, &clilen);
        } while (cli_addr.sin_port != addr_actif->sin_port);

        // 4. Comparaison (4 critères)
        int crit = rx_pkt.choixCritere;
        int val1 = 0, val2 = 0;
        char nom_stat[30];

        switch(crit) {
            case 1: val1 = c1.critere1; val2 = c2.critere1; strcpy(nom_stat, "Vitesse"); break;
            case 2: val1 = c1.critere2; val2 = c2.critere2; strcpy(nom_stat, "Puissance"); break;
            case 3: val1 = c1.critere3; val2 = c2.critere3; strcpy(nom_stat, "Cylindree"); break;
            case 4: val1 = c1.critere4; val2 = c2.critere4; strcpy(nom_stat, "RPM"); break;
            default: val1 = 0; val2 = 0; strcpy(nom_stat, "Inconnu");
        }

        int win = (val1 >= val2);

        // 5. Résolution
        if (win) {
            sprintf(paquet.texteInfo, "GAGNE ! %s : %d vs %d (%s)", nom_stat, val1, val2, c2.nom);
            ajouter_carte(t_actif, nt_actif, c1);
            ajouter_carte(t_actif, nt_actif, c2);
        } else {
            sprintf(paquet.texteInfo, "PERDU... %s : %d vs %d (%s)", nom_stat, val1, val2, c2.nom);
            ajouter_carte(t_passif, nt_passif, c1);
            ajouter_carte(t_passif, nt_passif, c2);
            tour_j1 = !tour_j1;
        }

        supprimer_top_carte(deck_j1, &nb_cartes_j1);
        supprimer_top_carte(deck_j2, &nb_cartes_j2);

        // 6. Envoi Résultats
        paquet.type = TYPE_RESULTAT; // texteInfo déjà rempli
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)addr_actif, clilen);

        if (win) strcpy(paquet.texteInfo, "L'adversaire a perdu la manche.");
        else strcpy(paquet.texteInfo, "VOUS AVEZ GAGNE la manche !");
        sendto(sockfd, &paquet, sizeof(paquet), 0, (struct sockaddr *)addr_passif, clilen);
    }

    printf("[GAME PID %d] Fin de partie.\n", getpid());
    close(sockfd);
    exit(0);
}

// --- MAIN SERVER (LOBBY) ---
int main(int argc, char *argv[]) {
    int main_sockfd, new_sockfd, portno;
    struct sockaddr_in serv_addr, cli_addr, j1_addr;
    socklen_t clilen = sizeof(cli_addr);
    GamePacket buffer;

    // Gestion Zombies
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) error("sigaction");

    if (argc < 2) { fprintf(stderr, "Port manquant\n"); exit(1); }

    main_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (main_sockfd < 0) error("ERREUR socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(main_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERREUR bind");

    printf("[SERVER UDP] Lobby ouvert sur port %d. Attente...\n", portno);

    int waiting_j1 = 0;

    while (1) {
        bzero(&buffer, sizeof(buffer));
        if (recvfrom(main_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &clilen) < 0)
            continue;

        if (!waiting_j1) {
            j1_addr = cli_addr;
            waiting_j1 = 1;
            printf("[LOBBY] J1 connecte. Attente de J2...\n");

            buffer.type = TYPE_ATTENTE;
            strcpy(buffer.texteInfo, "Bienvenue. Attente d'un adversaire...");
            sendto(main_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&j1_addr, clilen);
        } else {
            if (cli_addr.sin_port == j1_addr.sin_port) continue;

            printf("[LOBBY] J2 connecte. Creation de partie...\n");

            // Creation nouveau socket pour la partie
            new_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (new_sockfd < 0) error("Socket jeu");

            struct sockaddr_in game_addr;
            bzero(&game_addr, sizeof(game_addr));
            game_addr.sin_family = AF_INET;
            game_addr.sin_addr.s_addr = INADDR_ANY;
            game_addr.sin_port = 0; // Port aléatoire

            if (bind(new_sockfd, (struct sockaddr *)&game_addr, sizeof(game_addr)) < 0)
                error("Bind jeu");

            socklen_t len = sizeof(game_addr);
            getsockname(new_sockfd, (struct sockaddr *)&game_addr, &len);
            int new_port = ntohs(game_addr.sin_port);

            printf("[LOBBY] Nouvelle salle sur port %d\n", new_port);

            // Redirection
            buffer.type = TYPE_REDIRECT;
            sprintf(buffer.texteInfo, "%d", new_port);

            sendto(main_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&j1_addr, clilen);
            sendto(main_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, clilen);

            // Fork
            int pid = fork();
            if (pid < 0) error("Fork");

            if (pid == 0) {
                close(main_sockfd);
                jouer_partie(new_sockfd, j1_addr, cli_addr);
            } else {
                close(new_sockfd);
                waiting_j1 = 0;
                printf("[LOBBY] Partie lancee (PID %d). Retour lobby.\n", pid);
            }
        }
    }
    return 0;
}

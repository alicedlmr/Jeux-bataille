/*
 * server_tcp.c - Version MULTI-CLIENTS (FORK)
 * Jeu : Bataille de Voitures (4 Critères)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h> 
#include <signal.h>
#include <sys/wait.h>
#include "../base/jeux.h" 

// --- CONSTANTES ---
#define TOTAL_CARTES_JEU 20
#define MAIN_JOUEUR 10

// --- UTILITAIRES ---
void error(const char *msg) { perror(msg); exit(1); }

// Gestion des processus zombies
void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
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
    for (int i = 0; i < *nb_cartes - 1; i++) deck[i] = deck[i+1];
    (*nb_cartes)--;
}

void set_c(Carte *c, char *nom, int vit, int ch, int cc, int rpm) {
    strcpy(c->nom, nom);
    c->critere1 = vit; 
    c->critere2 = ch;  
    c->critere3 = cc;  
    c->critere4 = rpm; 
}

// --- DONNÉES DU JEU ---
Carte master_deck[TOTAL_CARTES_JEU];

void charger_voitures() {
    set_c(&master_deck[0], "Dodge Viper GTS", 306, 450, 7990, 5600);
    set_c(&master_deck[1], "Shelby Cobra 427", 260, 425, 7000, 6000);
    set_c(&master_deck[2], "Chevrolet Corvette C6", 300, 430, 6200, 6500);
    set_c(&master_deck[3], "Plymouth Barracuda", 220, 425, 7200, 5000);
    set_c(&master_deck[4], "Ford Mustang GT", 250, 460, 5000, 6500);
    set_c(&master_deck[5], "Honda S2000", 240, 240, 1997, 9000);
    set_c(&master_deck[6], "Mazda RX-7", 250, 280, 1308, 8000);
    set_c(&master_deck[7], "Nissan Skyline R34", 250, 320, 2600, 8000);
    set_c(&master_deck[8], "Toyota Supra MK4", 250, 330, 3000, 6800);
    set_c(&master_deck[9], "Subaru Impreza WRX", 230, 280, 2500, 7000);
    set_c(&master_deck[10], "Ferrari F40", 324, 478, 2900, 7000);
    set_c(&master_deck[11], "Porsche 911 GT3", 310, 500, 4000, 8500);
    set_c(&master_deck[12], "Lamborghini Diablo", 325, 492, 5700, 7000);
    set_c(&master_deck[13], "Audi R8 V10", 316, 525, 5200, 8000);
    set_c(&master_deck[14], "McLaren F1", 386, 627, 6100, 7500);
    set_c(&master_deck[15], "Lotus Exige", 240, 220, 1800, 8500);
    set_c(&master_deck[16], "BMW M3 E46", 250, 343, 3200, 7900);
    set_c(&master_deck[17], "Aston Martin DB9", 300, 450, 5900, 6000);
    set_c(&master_deck[18], "TVR Sagaris", 298, 400, 4000, 7000);
    set_c(&master_deck[19], "Fiat 500 Abarth", 210, 160, 1400, 5500);
}

void init_jeu_local(Carte *d1, int *n1, Carte *d2, int *n2) {
    charger_voitures();
    melanger_deck(master_deck, TOTAL_CARTES_JEU);
    
    *n1 = 0; *n2 = 0;
    for(int i=0; i < MAIN_JOUEUR * 2; i++) {
        if (i % 2 == 0) ajouter_carte(d1, n1, master_deck[i]);
        else ajouter_carte(d2, n2, master_deck[i]);
    }
}

// --- LOGIQUE PARTIE (PROCESSUS FILS) ---
void jouer_partie(int sock_j1, int sock_j2) {
    GamePacket paquet;
    
    Carte deck_j1[TOTAL_CARTES_JEU], deck_j2[TOTAL_CARTES_JEU];
    Carte temp_j1[TOTAL_CARTES_JEU], temp_j2[TOTAL_CARTES_JEU];
    int nb_cartes_j1 = 0, nb_cartes_j2 = 0;
    int nb_temp_j1 = 0, nb_temp_j2 = 0;

    init_jeu_local(deck_j1, &nb_cartes_j1, deck_j2, &nb_cartes_j2);
    printf("[PID %d] Partie commencee.\n", getpid());

    int tour_j1 = 1; 
    int partie_active = 1;

    while (partie_active) {
        // VICTOIRE ?
        if (nb_cartes_j1 == 0) {
            if (nb_temp_j1 > 0) recycler_deck(deck_j1, &nb_cartes_j1, temp_j1, &nb_temp_j1);
            else { 
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 2 !");
                write(sock_j1, &paquet, sizeof(paquet)); write(sock_j2, &paquet, sizeof(paquet));
                break;
            }
        }
        if (nb_cartes_j2 == 0) {
            if (nb_temp_j2 > 0) recycler_deck(deck_j2, &nb_cartes_j2, temp_j2, &nb_temp_j2);
            else { 
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 1 !");
                write(sock_j1, &paquet, sizeof(paquet)); write(sock_j2, &paquet, sizeof(paquet));
                break;
            }
        }

        int sock_actif = (tour_j1) ? sock_j1 : sock_j2;
        int sock_passif = (tour_j1) ? sock_j2 : sock_j1;
        Carte *d_actif = (tour_j1) ? deck_j1 : deck_j2;
        Carte *d_passif = (tour_j1) ? deck_j2 : deck_j1;
        Carte *t_actif = (tour_j1) ? temp_j1 : temp_j2;
        Carte *t_passif = (tour_j1) ? temp_j2 : temp_j1;
        int *nt_actif = (tour_j1) ? &nb_temp_j1 : &nb_temp_j2;
        int *nt_passif = (tour_j1) ? &nb_temp_j2 : &nb_temp_j1;

        Carte c1 = d_actif[0];
        Carte c2 = d_passif[0];

        // ENVOI DONNEES
        bzero((char *) &paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1;
        sprintf(paquet.texteInfo, "A vous ! (Cartes: %d)", (tour_j1 ? nb_cartes_j1 : nb_cartes_j2));
        write(sock_actif, &paquet, sizeof(paquet));

        paquet.type = TYPE_ADVERSAIRE;
        paquet.carteInfo = c2;
        sprintf(paquet.texteInfo, "L'adversaire joue...");
        write(sock_passif, &paquet, sizeof(paquet));

        // LECTURE
        if (read(sock_actif, &paquet, sizeof(paquet)) <= 0) break;
        int critere = paquet.choixCritere;

        // COMPARAISON
        int val1 = 0, val2 = 0;
        char nom_stat[30];

        switch(critere) {
            case 1: val1 = c1.critere1; val2 = c2.critere1; strcpy(nom_stat, "Vitesse"); break;
            case 2: val1 = c1.critere2; val2 = c2.critere2; strcpy(nom_stat, "Puissance"); break;
            case 3: val1 = c1.critere3; val2 = c2.critere3; strcpy(nom_stat, "Cylindree"); break;
            case 4: val1 = c1.critere4; val2 = c2.critere4; strcpy(nom_stat, "RPM"); break;
            default: val1 = 0; val2 = 0; strcpy(nom_stat, "Inconnu");
        }

        int j_actif_gagne = (val1 >= val2);

        // RESOLUTION
        if (j_actif_gagne) {
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
        supprimer_top_carte(deck_j2, &nb_cartes_j2); // <--- L'erreur était probablement ici

        // RESULTATS
        paquet.type = TYPE_RESULTAT; 
        write(sock_actif, &paquet, sizeof(paquet));

        if(j_actif_gagne) strcpy(paquet.texteInfo, "L'adversaire a gagne la manche.");
        else strcpy(paquet.texteInfo, "Vous avez gagne la manche !");
        write(sock_passif, &paquet, sizeof(paquet));

    } // FIN WHILE

    printf("[PID %d] Fin.\n", getpid());
    close(sock_j1); close(sock_j2);
    exit(0);
} // FIN FONCTION JOUER_PARTIE

// --- MAIN ---
int main(int argc, char *argv[]) {
    int sockfd, newsockfd1, newsockfd2, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int opt = 1;

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) exit(1);

    if (argc < 2) { fprintf(stderr,"Usage: %s port\n", argv[0]); exit(1); }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR socket");
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR bind");

    listen(sockfd, 10);
    clilen = sizeof(cli_addr);
    
    printf("[SERVEUR TCP] Pret sur le port %d (Mode Voitures)...\n", portno);

    while (1) {
        newsockfd1 = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd1 < 0) continue;
        printf("[LOBBY] J1 connecte. Attente J2...\n");

        newsockfd2 = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd2 < 0) { close(newsockfd1); continue; }
        printf("[LOBBY] J2 connecte. Fork...\n");

        int pid = fork();
        if (pid < 0) error("ERROR fork");
        
        if (pid == 0) {
            close(sockfd);
            srand(time(NULL) ^ getpid());
            jouer_partie(newsockfd1, newsockfd2);
        } else {
            close(newsockfd1);
            close(newsockfd2);
        }
    }
    return 0; 
}

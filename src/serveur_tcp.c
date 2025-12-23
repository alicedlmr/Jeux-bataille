/*
 * server_tcp.c
 * Version "Bataille Classique" : 
 * - Carte du dessus jouée automatiquement
 * - Recyclage du paquet gagné quand la main est vide
 * - Basé sur les sockets 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h> 
#include "../base/jeux.h" 

// --- CONSTANTES ---
#define TOTAL_CARTES_JEU 10
#define MAIN_JOUEUR 5

// --- VARIABLES GLOBALES ---
Carte master_deck[TOTAL_CARTES_JEU];

// Decks Principaux (ce qu'on joue)
Carte deck_j1[TOTAL_CARTES_JEU]; 
Carte deck_j2[TOTAL_CARTES_JEU];
int nb_cartes_j1 = 0;
int nb_cartes_j2 = 0;

// Decks Temporaires (ce qu'on gagne)
Carte temp_j1[TOTAL_CARTES_JEU];
Carte temp_j2[TOTAL_CARTES_JEU];
int nb_temp_j1 = 0;
int nb_temp_j2 = 0;

// --- FONCTIONS UTILITAIRES ---

void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Mélange un paquet
void melanger_deck(Carte *deck, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Carte temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// Transfère le temporaire vers le principal et mélange
void recycler_deck(Carte *main_deck, int *nb_main, Carte *temp_deck, int *nb_temp) {
    printf("[SERVEUR] Recyclage du deck (Gains -> Main)...\n");
    for(int i=0; i < *nb_temp; i++) {
        main_deck[i] = temp_deck[i];
    }
    *nb_main = *nb_temp;
    *nb_temp = 0; // On vide le temporaire
    
    melanger_deck(main_deck, *nb_main);
}

// Ajoute une carte au fond d'un paquet
void ajouter_carte(Carte *deck, int *nb_cartes, Carte c) {
    if (*nb_cartes < TOTAL_CARTES_JEU) {
        deck[*nb_cartes] = c;
        (*nb_cartes)++;
    }
}

// Supprime la carte du dessus (index 0) et décale tout
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

// --- DONNEES THEME 1 : VOITURES ---
void charger_voitures() {
    printf("[SERVEUR] Chargement du theme VOITURES.\n");
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

// --- DONNEES THEME 2 : ANIMAUX ---
void charger_animaux() {
    printf("[SERVEUR] Chargement du theme ANIMAUX.\n");
    // Critère 1 : Poids (kg), Critère 2 : Longevité (années)
    set_carte(&master_deck[0], "Elephant", 6000, 70);
    set_carte(&master_deck[1], "Lion", 190, 14);
    set_carte(&master_deck[2], "Tortue Geante", 250, 150);
    set_carte(&master_deck[3], "Aigle", 5, 20);
    set_carte(&master_deck[4], "Baleine Bleue", 150000, 90);
    set_carte(&master_deck[5], "Chien", 30, 13);
    set_carte(&master_deck[6], "Hamster", 1, 3);
    set_carte(&master_deck[7], "Crocodile", 500, 70);
    set_carte(&master_deck[8], "Cheval", 400, 25);
    set_carte(&master_deck[9], "Humain", 80, 85); // ;)
}

// --- LOGIQUE D'INITIALISATION ---
void init_jeu(int choix_theme) {
    // 1. Chargement selon le choix
    if (choix_theme == 1) charger_voitures();
    else charger_animaux();

    // 2. Mélange
    melanger_deck(master_deck, TOTAL_CARTES_JEU);

    // 3. Distribution
    nb_cartes_j1 = 0; nb_cartes_j2 = 0;
    nb_temp_j1 = 0; nb_temp_j2 = 0;

    for(int i=0; i < MAIN_JOUEUR; i++) {
        ajouter_carte(deck_j1, &nb_cartes_j1, master_deck[i]);
        ajouter_carte(deck_j2, &nb_cartes_j2, master_deck[i + MAIN_JOUEUR]);
    }
}

// --- MAIN ---
int main(int argc, char *argv[]) {
    int sockfd, newsockfd, newsockfd2, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int opt = 1;

    srand(time(NULL)); 

    if (argc < 2) { fprintf(stderr,"Port manquant\n"); exit(1); }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    printf("[SERVEUR] Attente sur port %d...\n", portno);

    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    printf("[SERVEUR] J1 connecte.\n");
    newsockfd2 = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    printf("[SERVEUR] J2 connecte.\n");

// --- PHASE DE VOTE ---
    GamePacket p_vote;
    bzero((char *) &p_vote, sizeof(p_vote));
    p_vote.type = TYPE_VOTE;
    strcpy(p_vote.texteInfo, "Votez pour le theme :\n1. Voitures (Vitesse/Puissance)\n2. Animaux (Poids/Longevite)");

    // Envoi de la question aux deux
    write(newsockfd, &p_vote, sizeof(p_vote));
    write(newsockfd2, &p_vote, sizeof(p_vote));

    // Réception des votes
    int vote1 = 1, vote2 = 1;
    
    // Lecture J1
    if (read(newsockfd, &p_vote, sizeof(p_vote)) > 0) vote1 = p_vote.choixCritere;
    // Lecture J2
    if (read(newsockfd2, &p_vote, sizeof(p_vote)) > 0) vote2 = p_vote.choixCritere;

    printf("[VOTE] J1 veut %d, J2 veut %d\n", vote1, vote2);

    // Décision
    int theme_final = 1;
    if (vote1 == vote2) {
        theme_final = vote1; // Accord
    } else {
        // Désaccord : le serveur tranche au hasard
        theme_final = (rand() % 2) + 1;
        printf("[VOTE] Desaccord. Le hasard choisit le theme %d\n", theme_final);
    }

    // Lancement avec le thème choisi
    init_jeu(theme_final);

    // --- BOUCLE DE JEU ---
    int tour_j1 = 1; 
    int partie_active = 1;
    GamePacket paquet;

    while (partie_active) {
        
        // 1. GESTION DES PAQUETS VIDES (RECYCLAGE)
        if (nb_cartes_j1 == 0) {
            if (nb_temp_j1 > 0) recycler_deck(deck_j1, &nb_cartes_j1, temp_j1, &nb_temp_j1);
            else { /* PERDU */ 
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 2 !");
                write(newsockfd, &paquet, sizeof(paquet)); write(newsockfd2, &paquet, sizeof(paquet));
                break;
            }
        }
        if (nb_cartes_j2 == 0) {
            if (nb_temp_j2 > 0) recycler_deck(deck_j2, &nb_cartes_j2, temp_j2, &nb_temp_j2);
            else { /* PERDU */
                paquet.type = TYPE_FIN; strcpy(paquet.texteInfo, "VICTOIRE DU JOUEUR 1 !");
                write(newsockfd, &paquet, sizeof(paquet)); write(newsockfd2, &paquet, sizeof(paquet));
                break;
            }
        }

        // 2. PREPARATION DU TOUR
        int sock_actif = (tour_j1) ? newsockfd : newsockfd2;
        int sock_passif = (tour_j1) ? newsockfd2 : newsockfd;
        Carte *deck_actif = (tour_j1) ? deck_j1 : deck_j2;
        Carte *deck_passif = (tour_j1) ? deck_j2 : deck_j1;
        
        // Pointeurs vers les decks temporaires (pour stocker les gains)
        Carte *temp_actif = (tour_j1) ? temp_j1 : temp_j2;
        Carte *temp_passif = (tour_j1) ? temp_j2 : temp_j1;
        int *nb_temp_actif = (tour_j1) ? &nb_temp_j1 : &nb_temp_j2;
        int *nb_temp_passif = (tour_j1) ? &nb_temp_j2 : &nb_temp_j1;

        // 3. ENVOI DES CARTES (Automatique : Index 0)
        Carte c1 = deck_actif[0]; // Carte du dessus
        Carte c2 = deck_passif[0];

        // Joueur Actif : Reçoit sa carte et doit choisir le critère
        bzero((char *) &paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1;
        sprintf(paquet.texteInfo, "C'est votre tour ! (Reste %d cartes + %d gagnées)", 
                (tour_j1 ? nb_cartes_j1 : nb_cartes_j2), (tour_j1 ? nb_temp_j1 : nb_temp_j2));
        write(sock_actif, &paquet, sizeof(paquet));

        // Joueur Passif : Attend
        paquet.type = TYPE_ADVERSAIRE;
        sprintf(paquet.texteInfo, "L'adversaire choisi le critère");
        write(sock_passif, &paquet, sizeof(paquet));

        // 4. LECTURE DU CHOIX
        if (read(sock_actif, &paquet, sizeof(paquet)) <= 0) break;
        int critere = paquet.choixCritere;

        // 5. RESOLUTION
        int val1 = (critere == 1) ? c1.critere1 : c1.critere2;
        int val2 = (critere == 1) ? c2.critere1 : c2.critere2;

        char msg[128];
        int j_actif_gagne = 0;

        if (val1 >= val2) j_actif_gagne = 1; // Egalité = victoire attaquant ici

        if (j_actif_gagne) {
            sprintf(msg, "Gagne ! %d vs %d (%s)", val1, val2, c2.nom);
            // Le gagnant (Actif) met les deux cartes dans son temporaire
            ajouter_carte(temp_actif, nb_temp_actif, c1);
            ajouter_carte(temp_actif, nb_temp_actif, c2);
        } else {
            sprintf(msg, "Perdu... %d vs %d (%s)", val1, val2, c2.nom);
            // Le gagnant (Passif) met les deux cartes dans son temporaire
            ajouter_carte(temp_passif, nb_temp_passif, c1);
            ajouter_carte(temp_passif, nb_temp_passif, c2);
            
            tour_j1 = !tour_j1; // Changement de main
        }

        // On retire les cartes jouées des mains principales
        supprimer_top_carte(deck_j1, &nb_cartes_j1);
        supprimer_top_carte(deck_j2, &nb_cartes_j2);

        // 6. RESULTATS
        paquet.type = TYPE_RESULTAT;
        strcpy(paquet.texteInfo, msg);
        write(sock_actif, &paquet, sizeof(paquet));

        if(j_actif_gagne) strcpy(paquet.texteInfo, "L'adversaire a gagne la manche.");
        else strcpy(paquet.texteInfo, "Vous avez gagne la manche !");
        write(sock_passif, &paquet, sizeof(paquet));

        printf("J1: %d(+%d) | J2: %d(+%d)\n", nb_cartes_j1, nb_temp_j1, nb_cartes_j2, nb_temp_j2);
    }

    close(newsockfd); close(newsockfd2); close(sockfd);
    return 0;
}
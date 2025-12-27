/*
 * server_tcp.c - Versão MULTI-CLIENTES (FORK)
 * O servidor aceita pares de jogadores e cria um processo filho para cada partida.
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
#define TOTAL_CARTES_JEU 10
#define MAIN_JOUEUR 5

// --- ESTRUTURAS ---
// (Definimos aqui para uso nas funções, mas as instâncias serão locais no fork)

// --- FUNÇÕES UTILITÁRIAS ---
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Handler para limpar processos zumbis (filhos que terminaram)
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
    // printf("[GAME INFO] Recyclage du deck...\n");
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

// --- CARGA DE DADOS ---
Carte master_deck[TOTAL_CARTES_JEU]; // Usado apenas como template

void charger_voitures() {
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

// Inicializa decks LOCAIS (para garantir thread-safety no fork)
void init_jeu_local(int theme, Carte *d1, int *n1, Carte *d2, int *n2) {
    if (theme == 1) charger_voitures();
    else charger_animaux();

    melanger_deck(master_deck, TOTAL_CARTES_JEU);
    *n1 = 0; *n2 = 0;

    for(int i=0; i < MAIN_JOUEUR; i++) {
        ajouter_carte(d1, n1, master_deck[i]);
        ajouter_carte(d2, n2, master_deck[i + MAIN_JOUEUR]);
    }
}

// --- LÓGICA DO JOGO (ISOLADA) ---
// Esta função roda dentro do PROCESSO FILHO
void jouer_partie(int sock_j1, int sock_j2) {
    GamePacket paquet;
    
    // Variáveis locais para este jogo específico
    Carte deck_j1[TOTAL_CARTES_JEU], deck_j2[TOTAL_CARTES_JEU];
    Carte temp_j1[TOTAL_CARTES_JEU], temp_j2[TOTAL_CARTES_JEU];
    int nb_cartes_j1 = 0, nb_cartes_j2 = 0;
    int nb_temp_j1 = 0, nb_temp_j2 = 0;

    // --- PHASE DE VOTE ---
    bzero((char *) &paquet, sizeof(paquet));
    paquet.type = TYPE_VOTE;
    strcpy(paquet.texteInfo, "Votez pour le theme :\n1. Voitures\n2. Animaux");

    write(sock_j1, &paquet, sizeof(paquet));
    write(sock_j2, &paquet, sizeof(paquet));

    int vote1 = 1, vote2 = 1;
    // Resposta J1
    if (read(sock_j1, &paquet, sizeof(paquet)) > 0) vote1 = paquet.choixCritere;
    // Resposta J2
    if (read(sock_j2, &paquet, sizeof(paquet)) > 0) vote2 = paquet.choixCritere;

    int theme_final = (vote1 == vote2) ? vote1 : (rand() % 2) + 1;
    
    // Inicializa o jogo
    init_jeu_local(theme_final, deck_j1, &nb_cartes_j1, deck_j2, &nb_cartes_j2);
    printf("[PID %d] Partie commencee. Theme: %d\n", getpid(), theme_final);

    // --- BOUCLE DE JEU ---
    int tour_j1 = 1; 
    int partie_active = 1;

    while (partie_active) {
        // Verifica FIM DE JOGO e Reciclagem
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

        // Ponteiros
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

        // Envio Info Carta
        bzero((char *) &paquet, sizeof(paquet));
        paquet.type = TYPE_TON_TOUR;
        paquet.carteInfo = c1;
        sprintf(paquet.texteInfo, "C'est votre tour ! (Reste %d cartes)", (tour_j1 ? nb_cartes_j1 : nb_cartes_j2));
        write(sock_actif, &paquet, sizeof(paquet));

        paquet.type = TYPE_ADVERSAIRE;
        sprintf(paquet.texteInfo, "L'adversaire reflechit...");
        write(sock_passif, &paquet, sizeof(paquet));

        // Leitura Escolha
        if (read(sock_actif, &paquet, sizeof(paquet)) <= 0) break;
        int critere = paquet.choixCritere;

        int val1 = (critere == 1) ? c1.critere1 : c1.critere2;
        int val2 = (critere == 1) ? c2.critere1 : c2.critere2;
        int j_actif_gagne = (val1 >= val2);

        if (j_actif_gagne) {
            sprintf(paquet.texteInfo, "Gagne ! %d vs %d (%s)", val1, val2, c2.nom);
            ajouter_carte(t_actif, nt_actif, c1);
            ajouter_carte(t_actif, nt_actif, c2);
        } else {
            sprintf(paquet.texteInfo, "Perdu... %d vs %d (%s)", val1, val2, c2.nom);
            ajouter_carte(t_passif, nt_passif, c1);
            ajouter_carte(t_passif, nt_passif, c2);
            tour_j1 = !tour_j1;
        }

        supprimer_top_carte(deck_j1, &nb_cartes_j1);
        supprimer_top_carte(deck_j2, &nb_cartes_j2);

        // Resultados
        paquet.type = TYPE_RESULTAT; 
        write(sock_actif, &paquet, sizeof(paquet));

        if(j_actif_gagne) strcpy(paquet.texteInfo, "L'adversaire a gagne la manche.");
        else strcpy(paquet.texteInfo, "Vous avez gagne la manche !");
        write(sock_passif, &paquet, sizeof(paquet));
    }

    printf("[PID %d] Fin de partie. Fermeture des sockets.\n", getpid());
    close(sock_j1);
    close(sock_j2);
    exit(0); // Mata o processo filho
}

// --- MAIN SERVER ---
int main(int argc, char *argv[]) {
    int sockfd, newsockfd1, newsockfd2, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int opt = 1;

    // Configuração para evitar processos zumbis
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

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

    listen(sockfd, 10);
    clilen = sizeof(cli_addr);
    
    printf("[SERVEUR TCP] En attente de connexions sur le port %d...\n", portno);

    // --- LOOP PRINCIPAL (LOBBY) ---
    while (1) {
        // 1. Aceita Jogador 1
        newsockfd1 = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd1 < 0) {
            perror("ERROR on accept");
            continue;
        }
        printf("[LOBBY] J1 connecte. En attente de J2...\n");

        // Opcional: Avisar J1 que estamos esperando (precisa adaptar cliente se descomentar)
        // GamePacket msg; msg.type = TYPE_ADVERSAIRE; strcpy(msg.texteInfo, "Attente adversaire...");
        // write(newsockfd1, &msg, sizeof(msg));

        // 2. Aceita Jogador 2
        newsockfd2 = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd2 < 0) {
            perror("ERROR on accept J2");
            close(newsockfd1);
            continue; 
        }
        printf("[LOBBY] J2 connecte. Lancement de la partie...\n");

        // 3. Fork para criar a sala de jogo
        int pid = fork();

        if (pid < 0) {
            error("ERROR on fork");
        }
        
        if (pid == 0) {
            // --- PROCESSO FILHO ---
            close(sockfd); // Filho não precisa ouvir novas conexões
            srand(time(NULL) ^ getpid()); // Seed aleatório baseado no PID
            jouer_partie(newsockfd1, newsockfd2);
            exit(0); // Garante que o filho termina aqui
        } 
        else {
            // --- PROCESSO PAI ---
            // Pai fecha os sockets dos clientes (pois o filho já assumiu)
            // Isso é CRUCIAL para não vazar descritores de arquivo
            close(newsockfd1);
            close(newsockfd2);
            printf("[LOBBY] Partie cree (PID %d). Retour en ecoute...\n", pid);
        }
    } /* Fim do While */

    close(sockfd);
    return 0; 
}

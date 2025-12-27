/* * jeux.h
 * Ce fichier contient les structures partagées par le client et le serveur pour le jeux.
 * On l'inclu dans les fichiers: server.c et client.c.
 */

#ifndef JEUX_H
#define JEUX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- CONSTANTES ---
#define PORT 8080           // Port de connexion par défaut
#define BUFFER_SIZE 1024    // Taille standard des tampons
#define MAX_NAME 50         // Taille max pour le nom d'une carte
#define TEXT_SIZE 512

// --- CODES DES MESSAGES (Protocol) ---
#define TYPE_VOTE 'V' // V = Vote pour le thème
#define TYPE_ATTENTE    'A'  // A = Attente d'un adversaire
#define TYPE_TON_TOUR   'T'  // T = Ton Tour de jouer
#define TYPE_ADVERSAIRE 'W'  // W = Wait (l'adversaire joue)
#define TYPE_CHOIX      'C'  // C = Choix du critère envoyé par le joueur
#define TYPE_RESULTAT   'R'  // R = Résultat de la manche
#define TYPE_FIN        'F'  // F = Fin de la partie
#define TYPE_REDIRECT       'D'  // D = Déviation
#define TYPE_SELECTION_CARTE 'S' // S = Selection

// --- STRUCTURE CARTE (TOP TRUMPS) ---
typedef struct {
    char nom[MAX_NAME]; // Nom de la carte
    int critere1;        // Critère 1
    int critere2;      // Critère 2
    int critere3;          // Critère 3
    int critere4;   // Critère 4
} Carte;

// --- STRUCTURE DU PAQUET RESEAU ---
typedef struct __attribute__((packed)) {
    char type;              // Le code message ('A', 'T', 'C'...)
    Carte carteInfo;        // Les infos de la carte (si nécessaire)
    int choixCritere;       // L'indice du critère choisi (1-4)
    char texteInfo[TEXT_SIZE];    // Message texte pour l'affichage
} GamePacket;

#endif

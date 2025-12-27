/* base/jeux.h */
#ifndef JEUX_H
#define JEUX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- CONSTANTES ---
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_NAME 50
#define TEXT_SIZE 512

// --- CODES PROTOCOLE ---
#define TYPE_VOTE       'V'
#define TYPE_ATTENTE    'A'
#define TYPE_TON_TOUR   'T'
#define TYPE_ADVERSAIRE 'W'
#define TYPE_CHOIX      'C'
#define TYPE_RESULTAT   'R'
#define TYPE_FIN        'F'
#define TYPE_REDIRECT   'D'

// --- STRUCTURE CARTE (4 CRITÈRES) ---
typedef struct {
    char nom[MAX_NAME];
    int critere1; // Vitesse Max (km/h)
    int critere2; // Puissance (ch)
    int critere3; // Cylindrée (cm3)
    int critere4; // Régime Max (tr/min)
} Carte;

// --- STRUCTURE PAQUET ---
typedef struct __attribute__((packed)) {
    char type;
    Carte carteInfo;
    int choixCritere;
    char texteInfo[TEXT_SIZE];
} GamePacket;

#endif

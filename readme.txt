===========================================================
      PROJET RÉSEAUX - JEU DE BATAILLE (TOP TRUMPS)
===========================================================

== 1. COMPILATION ==
Avant de commencer, compilez le projet à la racine :
$ make

== 2. VERSION TCP (Multi-clients) ==
Le serveur peut gérer plusieurs parties en même temps.

1. Lancer le serveur (Terminal 1) :
   $ ./server_tcp 8080

2. Lancer une partie (Terminaux 2 et 3) :
   $ ./client_tcp localhost 8080
   $ ./client_tcp localhost 8080

3. (Optionnel) Lancer une 2ème partie simultanée (Terminaux 4 et 5) :
   Réexécutez simplement ./client_tcp localhost 8080 dans deux nouvelles fenêtres.

== 3. VERSION UDP (Multi-clients) ==
Le serveur redirige automatiquement les joueurs vers des ports dédiés.

1. Lancer le serveur (Terminal 1) :
   $ ./server_udp 8080

2. Lancer les clients (Terminaux 2 et 3) :
   $ ./client_udp localhost 8080
   $ ./client_udp localhost 8080
   (Le jeu commencera automatiquement une fois la paire connectée)

== 4. COMMENT JOUER ==
- Le jeu est une bataille de caractéristiques de voitures.
- À votre tour, choisissez un critère (1 à 4) :
  1. Vitesse | 2. Puissance | 3. Cylindrée | 4. RPM
- La valeur la plus élevée remporte la manche.


Pour supprimer les exécutables :
$ make clean

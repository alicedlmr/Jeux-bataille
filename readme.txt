Le jeux de bataille se joue à deux joueurs. 


Pour lancer le jeux partie tcp :

-ouvrir 3 fenêtres de terminal (lancement du serveur, joueur 1 et joueur 2)
-se placer dans le dossier du projet dans les trois fenêtres
-executer dans la 1ere: gcc -I common src/serveur_tcp.c -o server 
                        gcc -I common src/client_tcp.c -o client
                        ./server 8080
-executer dans la 2eme: ./client localhost 8080
-executer dans la 3eme: ./client localhost 8080

Puis jouer

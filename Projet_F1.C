#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
// Fonction pour générer un temps aléatoire en minutes, secondes et millisecondes
void generateRandomTime(int *minutes, int *seconds, int *milliseconds) {
    int minSec = 25;
    int maxSec = 45;
    int totalSeconds = rand() % (maxSec - minSec + 1) + minSec;
    *milliseconds = rand() % 1000;
    *minutes = totalSeconds / 60;
    *seconds = totalSeconds % 60;
}

/*  // Fonction pour afficher le menu de configuration
void afficherMenu(int *nombreDeTours, int *nombreDeVoitures) {
    printf("===== Configuration du programme =====\n");

    // Demander le nombre de tours
    printf("Entrez le nombre de tours (entre 1 et 10) : ");
    scanf("%d", nombreDeTours);
    if (*nombreDeTours < 1 || *nombreDeTours > 10) {
        printf("Nombre de tours invalide. Utilisation de la valeur par défaut : 3 tours.\n");
        *nombreDeTours = 3;
    }

    // Demander le nombre de voitures
    printf("Entrez le nombre de voitures (entre 1 et 20) : ");
    scanf("%d", nombreDeVoitures);
    if (*nombreDeVoitures < 1 || *nombreDeVoitures > 20) {
        printf("Nombre de voitures invalide. Utilisation de la valeur par défaut : 20 voitures.\n");
        *nombreDeVoitures = 20;
    }

    printf("=====================================\n\n");
}*/

int main() {
    setlocale(LC_ALL, "");  // Pour accepter les accents
    srand(time(NULL));      // Initialisation de la graine aléatoire

    int nombreDeTours = 3;  // Par défaut, chaque voiture effectue 3 tours
    int nombreDeVoitures = 20; // Par défaut, 20 voitures
    int voiture[20] = {1, 11, 44, 63, 16, 55, 4, 81, 14, 18, 10, 31, 23, 2, 22, 3, 77, 24, 20, 27};

    // Affichage du menu pour permettre à l'utilisateur de paramétrer le programme
    // afficherMenu(&nombreDeTours, &nombreDeVoitures);

    // Tableau pour stocker les temps des tours pour chaque voiture
    int temps[20][10][3]; // Max 20 voitures, 10 tours, chaque tour a minutes, secondes, millisecondes

    // Générer et stocker les temps pour chaque voiture et chaque tour
    /*┌─┬─┐
      │ │ │
      ├─┼─┤
      │ │ │
      └─┴─┘
    */
    for (int i = 0; i < nombreDeVoitures; i++) {
        printf("┌────────────────────────────────────────┐\n");
        printf("│               Voiture %d               │\n", voiture[i]);
        printf("├─────────────────┬──────────────────────┤\n");

        // Variables pour le total du temps de la voiture
        int total_minutes = 0;
        int total_seconds = 0;
        int total_milliseconds = 0;

        // Génération des temps pour chaque tour
        for (int tour = 0; tour < nombreDeTours; tour++) {
            generateRandomTime(&temps[i][tour][0], &temps[i][tour][1], &temps[i][tour][2]); // minutes, seconds, milliseconds
            printf("│       S%d        │      %02d:%02d:%03d       │\n", tour + 1, temps[i][tour][0], temps[i][tour][1], temps[i][tour][2]);

            // Addition des temps des tours
            total_minutes += temps[i][tour][0];
            total_seconds += temps[i][tour][1];
            total_milliseconds += temps[i][tour][2];
        }

        // Gestion du dépassement des millisecondes et secondes
        total_seconds += total_milliseconds / 1000;  // Convertir les millisecondes en secondes
        total_milliseconds %= 1000;                  // Gérer le reste des millisecondes
        total_minutes += total_seconds / 60;         // Convertir les secondes en minutes
        total_seconds %= 60;                         // Gérer le reste des secondes

        // Affichage du temps total pour chaque voiture
        printf("├─────────────────┴──────────────────────┤\n");
        printf("│Temps total de la voiture %d : %02d:%02d:%03d│\n", voiture[i], total_minutes, total_seconds, total_milliseconds);
        printf("└────────────────────────────────────────┘\n");   
    }

    return 0;
}

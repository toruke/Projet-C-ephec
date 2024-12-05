#include <stdio.h>
#include <string.h>
#include <math.h>

#define DISTANCE_STANDARD 305.0
#define DISTANCE_MONACO 260.0

void calculer_nombre_tours(const char *fichier) {
    FILE *fp = fopen(fichier, "r");
    if (fp == NULL) {
        printf("Erreur : Impossible d'ouvrir le fichier.\n");
        return;
    }

    char ligne[100];
    char circuits[25][50]; // Stocker les noms des circuits
    double longueurs[25];  // Stocker les longueurs des circuits
    int total_circuits = 0;

    // Lire le fichier et remplir les tableaux
    while (fgets(ligne, sizeof(ligne), fp)) {
        char nom[50];
        double longueur;
        if (sscanf(ligne, "%[^:]: %lf km", nom, &longueur) != 2) {
            strcpy(circuits[total_circuits], nom);
            longueurs[total_circuits] = longueur;
            total_circuits++;
        }
    }
    fclose(fp);

    // Afficher les circuits disponibles
    printf("Liste des circuits disponibles :\n");
    for (int i = 0; i < total_circuits; i++) {
        printf("%d. %s (%.3f km)\n", i + 1, circuits[i], longueurs[i]);
    }

    // Demander à l'utilisateur de choisir un circuit
    int choix;
    printf("Entrez le numéro du circuit que vous voulez choisir : ");
    scanf("%d", &choix);

    if (choix < 1 || choix > total_circuits) {
        printf("Choix invalide.\n");
        return;
    }

    // Calculer le nombre de tours
    double distance = (strcmp(circuits[choix - 1], "Monaco (Monte-Carlo)") == 0) ? DISTANCE_MONACO : DISTANCE_STANDARD;
    double nombre_tours = ceil(distance / longueurs[choix - 1]);

    printf("Pour le circuit %s, vous devez faire %.0f tours pour atteindre une distance de %.0f km.\n",
           circuits[choix - 1], nombre_tours, distance);
}

int main() {
    // Nom du fichier contenant les circuits
    const char *fichier = "Circuits_F1_2024.txt";

    calculer_nombre_tours(fichier);
    return 0;
}

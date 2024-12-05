#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>  // Pour open
#include <unistd.h> // Pour close et read

#define DISTANCE_STANDARD 305.0
#define DISTANCE_MONACO 260.0

void calculer_nombre_tours(const char *fichier) {
    // Ouvrir le fichier en lecture
    int fd = open(fichier, O_RDONLY);
    if (fd == -1) {
        perror("Erreur : Impossible d'ouvrir le fichier");
        return;
    }

    char buffer[4096]; // Pour lire tout le fichier en mémoire
    ssize_t bytes_lus = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_lus <= 0) {
        perror("Erreur : Lecture du fichier échouée");
        close(fd);
        return;
    }
    buffer[bytes_lus] = '\0'; // Ajouter le caractère de fin de chaîne

    close(fd); // Fermer le fichier après lecture

    // Traiter le contenu lu
    char lignes[25][100];
    int total_circuits = 0;
    char *ligne = strtok(buffer, "\n"); // Diviser par ligne
    while (ligne && total_circuits < 25) {
        strcpy(lignes[total_circuits++], ligne);
        ligne = strtok(NULL, "\n");
    }

    char circuits[25][50];
    double longueurs[25];

    for (int i = 0; i < total_circuits; i++) {
        char nom[50];
        double longueur;
        if (sscanf(lignes[i], "%[^:]: %lf km", nom, &longueur) == 2) {
            strcpy(circuits[i], nom);
            longueurs[i] = longueur;
        }
    }

    // Afficher les circuits disponibles
    perror("Liste des circuits disponibles :\n");
    for (int i = 0; i < total_circuits; i++) {
        printf("%d. %s (%.3f km)\n", i + 1, circuits[i], longueurs[i]);
    }

    // Demander à l'utilisateur de choisir un circuit
    int choix;
    perror("Entrez le numéro du circuit que vous voulez choisir : ");
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

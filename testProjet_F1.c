#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>  // Pour open
#include <unistd.h> // Pour close et read

#define DISTANCE_STANDARD 305.0
#define DISTANCE_MONACO 260.0

void calculer_nombre_tours(const char *fichier) {
    // Ouvrir le fichier en mode lecture seule
    int fd = open(fichier, O_RDONLY);
    if (fd == -1) {
        perror("Erreur : Impossible d'ouvrir le fichier"); // Afficher l'erreur si le fichier ne peut pas être ouvert
        return;
    }

    char buffer[4096]; // Buffer pour lire tout le contenu du fichier en mémoire
    ssize_t bytes_lus = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_lus <= 0) {
        perror("Erreur : Lecture du fichier échouée"); // Afficher une erreur si la lecture échoue
        close(fd);
        return;
    }
    buffer[bytes_lus] = '\0'; // Ajouter le caractère de fin de chaîne pour utiliser le buffer comme une chaîne C

    close(fd); // Fermer le fichier après lecture

    // Diviser le contenu lu en lignes (chaque ligne correspond à un circuit)
    char lignes[25][100];
    int total_circuits = 0;
    char *ligne = strtok(buffer, "\n"); // Séparer les lignes du fichier
    while (ligne && total_circuits < 25) {
        strcpy(lignes[total_circuits++], ligne); // Copier chaque ligne dans le tableau `lignes`
        ligne = strtok(NULL, "\n");
    }

    char circuits[25][50];
    double longueurs[25];

    // Extraire les noms des circuits et leurs longueurs en kilomètres
    for (int i = 0; i < total_circuits; i++) {
        char nom[50];
        double longueur;
        if (sscanf(lignes[i], "%[^:]: %lf km", nom, &longueur) == 2) {
            strcpy(circuits[i], nom);     // Stocker le nom du circuit
            longueurs[i] = longueur;     // Stocker la longueur correspondante
        }
    }

    // Afficher les circuits disponibles à l'utilisateur
    printf("Liste des circuits disponibles :\n");
    for (int i = 0; i < total_circuits; i++) {
        printf("%d. %s (%.3f km)\n", i + 1, circuits[i], longueurs[i]);
    }

    // Demander à l'utilisateur de sélectionner un circuit
    int choix;
    printf("Entrez le numéro du circuit que vous voulez choisir : ");
    scanf("%d", &choix);

    if (choix < 1 || choix > total_circuits) {
        printf("Choix invalide.\n"); // Vérifier que l'utilisateur a choisi un numéro valide
        return;
    }

    // Calculer le nombre de tours nécessaires en fonction de la distance totale (Monaco ou standard)
    double distance = (strcmp(circuits[choix - 1], "Monaco (Monte-Carlo)") == 0) ? DISTANCE_MONACO : DISTANCE_STANDARD;
    double nombre_tours = ceil(distance / longueurs[choix - 1]); // Arrondir au tour supérieur

    // Afficher le résultat final
    printf("Pour le circuit %s, vous devez faire %.0f tours pour atteindre une distance de %.0f km.\n",
           circuits[choix - 1], nombre_tours, distance);
}

int main() {
    // Nom du fichier contenant les circuits
    const char *fichier = "Circuits_F1_2024.txt";

    // Lancer le calcul des tours pour les circuits
    calculer_nombre_tours(fichier);
    return 0;
}

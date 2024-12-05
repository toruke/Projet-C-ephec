#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>


//Choix du week-end de la course :
/*int main() {
    int choix;

    // Afficher le menu :
    printf("Quel week-end désirez-vous :\n");
    printf("1. Week-end normal\n");
    printf("2. Week-end spécial\n");

    // Demander un choix :
    printf("Choix : 1 ou 2 : ");
    scanf("%d", &choix);

    // Traitement du choix
    if (choix == 1) {
        printf("Vous avez choisi le week-end normal\n");
    }
    else if (choix == 2) {
        printf("Vous avez choisi le week-end spécial\n");
    }
    else {
        printf("Vous n'avez pas choisi de week-end valide\n");
    }

    return 0;
}*/

//Lecture des circuits :

/*#define DISTANCE_STANDARD 305.0
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
    printf("%s, vous devez faire %.0f tours \n",
           circuits[choix - 1], nombre_tours);
}

int main() {
    // Nom du fichier contenant les circuits
    const char *fichier = "Circuits_F1_2024.txt";

    // Lancer le calcul des tours pour les circuits
    calculer_nombre_tours(fichier);
    return 0;
}*/

// Faire tourner les voitures :

int voiture[20] = {1, 11, 44, 63, 16, 55, 4, 81, 14, 18, 10, 31, 23, 2, 22, 3, 77, 24, 20, 27};

// Fonction pour générer un temps aléatoire entre 20 et 45 secondes
void generateRandomTime(int *minutes, int *seconds) {
    int totalSeconds = rand() % 26 + 20;  // Générer un temps entre 20 et 45 secondes
    *minutes = totalSeconds / 60;
    *seconds = totalSeconds % 60;
}

// Fonction pour écrire les données dans un fichier CSV avec un tableau bien formaté
void writeToCSV(const char *filename, int voiture[], int nombreDeVoitures, int tours[20][3][2], int totalTemps[20][2]) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Erreur d'ouverture du fichier.\n");
        return;
    }

    // Écrire le tableau avec des en-têtes dans le fichier CSV
    fprintf(file, "+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");
    fprintf(file, "| Voiture |     S1 (mm:ss)     |     S2 (mm:ss)     |     S3 (mm:ss)     |  Total (mm:ss)     |\n");
    fprintf(file, "+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");

    // Écrire les temps pour chaque voiture
    for (int i = 0; i < nombreDeVoitures; i++) {
        fprintf(file, "|    %d    |", voiture[i]);

        // Écrire les temps des 3 tours pour chaque voiture
        for (int j = 0; j < 3; j++) {
            fprintf(file, "   %2d:%02d   |", tours[i][j][0], tours[i][j][1]);
        }

        // Afficher le temps total pour chaque voiture
        fprintf(file, "   %2d:%02d   |\n", totalTemps[i][0], totalTemps[i][1]);
        fprintf(file, "+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");
    }

    fclose(file);
    printf("Données écrites dans le fichier %s.\n", filename);
}

// Fonction pour afficher les données à l'écran avec un beau tableau
void displayData(int voiture[], int nombreDeVoitures, int tours[20][3][2], int totalTemps[20][2]) {
    // Affichage du tableau
    printf("+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");
    printf("| Voiture |     S1 (mm:ss)     |     S2 (mm:ss)     |     S3 (mm:ss)     |  Total (mm:ss)     |\n");
    printf("+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");

    // Affichage des données
    for (int i = 0; i < nombreDeVoitures; i++) {
        printf("|    %d    |", voiture[i]);

        // Afficher les temps des 3 tours pour chaque voiture
        for (int j = 0; j < 3; j++) {
            printf("   %2d:%02d   |", tours[i][j][0], tours[i][j][1]);
        }

        // Afficher le temps total pour chaque voiture
        printf("   %2d:%02d   |\n", totalTemps[i][0], totalTemps[i][1]);
        printf("+---------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+\n");
    }
}

// Fonction pour déterminer et afficher la voiture avec le meilleur temps
void displayBestTime(int voiture[], int totalTemps[20][2], int nombreDeVoitures) {
    int bestTimeMinutes = 1000, bestTimeSeconds = 1000;
    int bestCar = -1;

    // Trouver la voiture avec le meilleur temps
    for (int i = 0; i < nombreDeVoitures; i++) {
        int totalSeconds = totalTemps[i][0] * 60 + totalTemps[i][1];
        int bestTotalSeconds = bestTimeMinutes * 60 + bestTimeSeconds;

        if (totalSeconds < bestTotalSeconds) {
            bestTimeMinutes = totalTemps[i][0];
            bestTimeSeconds = totalTemps[i][1];
            bestCar = voiture[i];
        }
    }

    // Afficher la voiture avec le meilleur temps
    if (bestCar != -1) {
        printf("\nLa voiture avec le meilleur temps est la voiture %d avec un temps de %d:%02d.\n", bestCar, bestTimeMinutes, bestTimeSeconds);
    }
}

// Fonction pour trier les voitures par leur temps total (du plus petit au plus grand)
void sortCarsByTime(int voiture[], int totalTemps[20][2], int nombreDeVoitures) {
    // Tri par insertion des voitures en fonction de leur temps total (en secondes)
    for (int i = 1; i < nombreDeVoitures; i++) {
        int currentCar = voiture[i];
        int currentTimeInSeconds = totalTemps[i][0] * 60 + totalTemps[i][1];

        int j = i - 1;
        while (j >= 0 && (totalTemps[j][0] * 60 + totalTemps[j][1]) > currentTimeInSeconds) {
            voiture[j + 1] = voiture[j];
            totalTemps[j + 1][0] = totalTemps[j][0];
            totalTemps[j + 1][1] = totalTemps[j][1];
            j--;
        }

        voiture[j + 1] = currentCar;
        totalTemps[j + 1][0] = currentTimeInSeconds / 60;
        totalTemps[j + 1][1] = currentTimeInSeconds % 60;
    }
}

int main() {
    srand(time(NULL));  // Initialisation de la graine pour générer des nombres aléatoires

    int tours[20][3][2];  // Tableau pour stocker les temps (3 tours par voiture, minutes et secondes)
    int totalTemps[20][2];  // Tableau pour stocker le temps total par voiture (minutes, secondes)

    // Générer des temps pour chaque voiture
    for (int i = 0; i < 20; i++) {
        int totalMinutes = 0, totalSeconds = 0;
        for (int j = 0; j < 3; j++) {
            generateRandomTime(&tours[i][j][0], &tours[i][j][1]);  // Générer des temps pour chaque tour
            totalMinutes += tours[i][j][0];
            totalSeconds += tours[i][j][1];
        }

        // Calculer le total en minutes et secondes
        totalSeconds += totalMinutes * 60;
        totalTemps[i][0] = totalSeconds / 60;
        totalTemps[i][1] = totalSeconds % 60;
    }

    // Trier les voitures par leur temps total
    sortCarsByTime(voiture, totalTemps, 20);

    // Afficher les résultats à l'écran (triés)
    displayData(voiture, 20, tours, totalTemps);

    // Afficher la voiture avec le meilleur temps
    displayBestTime(voiture, totalTemps, 20);

    // Écrire les résultats dans un fichier CSV
    writeToCSV("voitures.csv", voiture, 20, tours, totalTemps);

    return 0;
}

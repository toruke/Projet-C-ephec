/*#include <stdio.h>
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
}*/

/*
 #include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <fcntl.h>  // Pour open
#include <unistd.h> // Pour close et read
#include <math.h>

#define DISTANCE_STANDARD 305.0
#define DISTANCE_MONACO 260.0

// Fonction pour générer un temps aléatoire en minutes, secondes et millisecondes
void generateRandomTime(int *minutes, int *seconds, int *milliseconds, int tour) {
    // Les temps de base pour un tour (25 à 45 secondes)
    int minSec = 25;
    int maxSec = 45;

    // Introduire un facteur qui augmente avec le nombre de tours effectués
    int progressionFactor = (tour + 1) * 2;  // Par exemple, chaque tour prend un peu plus de temps
    int totalSeconds = rand() % (maxSec - minSec + 1) + minSec + progressionFactor;

    // Calculer le temps en minutes, secondes et millisecondes
    *milliseconds = rand() % 1000;
    *minutes = totalSeconds / 60;
    *seconds = totalSeconds % 60;
}

// Fonction pour afficher le menu de configuration
void afficherMenu(int *nombreDeTours, int *nombreDeVoitures, int *choixCircuit) {
    printf("===== Configuration du programme =====\n");

    // Afficher les circuits disponibles
    printf("Choisissez un circuit :\n");
    printf("1. Circuit Standard (305 km)\n");
    printf("2. Monaco (260 km)\n");
    printf("Entrez votre choix (1 ou 2) : ");
    scanf("%d", choixCircuit);

    // Validation du choix du circuit
    if (*choixCircuit != 1 && *choixCircuit != 2) {
        printf("Choix invalide. Utilisation du circuit Standard par défaut.\n");
        *choixCircuit = 1;
    }

    printf("=====================================\n\n");
}

// Fonction pour calculer le nombre de tours en fonction du circuit choisi
void calculerNombreDeTours(const char *fichier, int choixCircuit) {
    // Ouvrir le fichier contenant les informations sur les circuits
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

    // Calculer le nombre de tours nécessaires en fonction du circuit choisi
    double distance = (choixCircuit == 2) ? DISTANCE_MONACO : DISTANCE_STANDARD;
    double nombre_tours = ceil(distance / longueurs[0]); // Arrondir au tour supérieur

    printf("Pour le circuit %s, vous devez faire %.0f tours pour atteindre une distance de %.0f km.\n",
           circuits[0], nombre_tours, distance);
}

int main() {
    setlocale(LC_ALL, "");  // Pour accepter les accents
    srand(time(NULL));      // Initialisation de la graine aléatoire

    int nombreDeTours = 3;  // Par défaut, chaque voiture effectue 3 tours
    int nombreDeVoitures = 20; // Par défaut, 20 voitures
    int choixCircuit = 1;   // Choix par défaut : circuit standard
    int voiture[20] = {1, 11, 44, 63, 16, 55, 4, 81, 14, 18, 10, 31, 23, 2, 22, 3, 77, 24, 20, 27};

    // Affichage du menu pour permettre à l'utilisateur de paramétrer le programme
    afficherMenu(&nombreDeTours, &nombreDeVoitures, &choixCircuit);

    // Nom du fichier contenant les circuits
    const char *fichier = "Circuits_F1_2024.txt";

    // Lancer le calcul des tours en fonction du circuit
    calculerNombreDeTours(fichier, choixCircuit);

    // Tableau pour stocker les temps des tours pour chaque voiture
    int temps[20][10][3]; // Max 20 voitures, 10 tours, chaque tour a minutes, secondes, millisecondes

    // Générer et stocker les temps pour chaque voiture et chaque tour
    for (int i = 0; i < nombreDeVoitures; i++) {
        printf("┌────────────────────────────────────────┐\n");
        if(voiture[i]<10){
            printf("│               Voiture %d                │\n", voiture[i]);
        }
        else{
            printf("│               Voiture %d               │\n", voiture[i]);
        }
        printf("├─────────────────┬──────────────────────┤\n");

        // Variables pour le total du temps de la voiture
        int total_minutes = 0;
        int total_seconds = 0;
        int total_milliseconds = 0;

        // Génération des temps pour chaque tour
        for (int tour = 0; tour < nombreDeTours; tour++) {
            generateRandomTime(&temps[i][tour][0], &temps[i][tour][1], &temps[i][tour][2], tour); // minutes, seconds, milliseconds
            printf("│       S%d        │      %02d:%02d:%03d       │\n", tour + 1, temps[i][tour][0], temps[i][tour][1], temps[i][tour][2]);
            if(tour != nombreDeTours - 1){
                printf("├─────────────────┼──────────────────────┤\n");
            }

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
        if(voiture[i]<10){
            printf("│Temps total de la voiture %d : %02d:%02d:%03d │\n", voiture[i], total_minutes, total_seconds, total_milliseconds);
        }
        else{
            printf("│Temps total de la voiture %d : %02d:%02d:%03d│\n", voiture[i], total_minutes, total_seconds, total_milliseconds);
        }
        printf("└────────────────────────────────────────┘\n");
    }

    return 0;
}
 */

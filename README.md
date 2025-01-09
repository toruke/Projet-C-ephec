# CourseF1Manager

## Usage
Le programme est écrit en C et n'utilise que les librairies standards.

Commande pour compiler le programme: `gcc course.c -o course`

Pour fonctionner, le programme a besoin de 2 fichiers: `drivers.csv` et `tracks.csv`.

Commande pour démarrer le programme: `./course`

Après une confirmation de l'utilisateur, le programme va simuler une phase de week-end (essai libre, qualification ou course), afficher les résultats et les enregister dans un ou plusieurs fichiers.  Au démarrage, il lira les fichiers déjà créés pour savoir où en est l'avancement du week-end et le classement des pilotes.

### drivers.csv
Le fichier contient 3 champs: le numéro du pilote, le nom court du pilote (les 3 permières lettres du nom de famille) et le nom complet (prénom nom). 
Le fichier doit contenir 20 lignes pour les 20 pilotes inscrits au championnat.

Exemple:
```
1;VER;Max Verstappen
11;PER;Sergio Pérez
44;HAM;Lewis Hamilton
```

### tracks.csv
Le fichier contient 4 champs: le pays, le nom du circuit, le type de week-end et la distance en mètre d'un tour de circuit.
Le type de week-end, peut-être soit "normal" soit "sprint".  Un week-end "normal" est composé de 3 essais libres, 3 séances de qualification et la course.
Un week-end "sprint" est composé d'un essai libre, 3 séances de qualification pour le sprint, une course "sprint", puis 3 séances de qualification classique et la course.
Le fichier doit contenir les informations pour les 22 circuits.

Exemple:
```
Bahrain;Bahrain International Circuit;normal;5412
Arabie Saoudite;Jeddah Corniche Circuit;normal;6174
Australie;Albert Park Circuit;normal;5278
Azerbaijan;Baku City Circuit;sprint;6003
```
### Les fichiers créés par le programme
Le programme va créé plusieurs fichiers lors des différentes exécutions, on peut les classer en 3 types:
* `championship.txt`: c'est le fichier qui indique la dernière course/phase exécutée.  Il contient 2 lignes: `race=n` et `phase=m`.  Au démarrage, le programme lit le fichier pour savoir quelle sera la phase suivante à simuler.  Si le fichier n'existe pas, on suppose que l'on est au début du championnat.
* `race_nn_pp.csv`: c'est le résultat de la simulation (nn est le numéro de la course et pp représente la phase (`F1` pour essai libre 1, `race` pour la course, ...).  On y retouve la liste des pilotes classés en fonction de leur résultat.  Il sera utilisé lors de certaines phases pour déterminer le classement des pilotes sur la piste de départ (qualification 2/3, sprint et course finale).  Il contient le numéro du pilote, son meilleur temps au tour et de chaque section.
* `race_nn_(race|sprint)_ranking.csv`: c'est le résultat du sprint ou de la course.  On y retrouve 2 informations: le numéro du pilote et le nombre de point marqué.  Ces fichiers sont lus par le programme à la fin de la simulation pour afficher le classement des pilotes.
 
## Description du programme
Ce programme simule un week-end de course de formule 1: à savoir les essais libres et qualifications ou la course (ou sprint).
Pour ce faire, le programme principal (`main`) va démarrer 3 types de sous-process (`controller`, `carSimulator`, `screenManager`) et attendre qu'ils se terminent.

Le premier sous-process est le `controller`, il est le composant central.  Il traite les données envoyées par les `carSimulator`s et les cumule pour le `screenManager` et pour le process `main` qui crée les fichiers résultats sur disque.

Le `screenManager` affiche à l'écran les données compilées par le controller.  Il rafraichit l'écran 1 fois par seconde.

Le `carSimulator` simule une voiture sur le circuit.  Le programme principal en démarre autant que de pilote en course (entre 10 et 20 en fonction de la phase du week-end).   Afin d'accélérer le temps, la vitesse des voitures est multipliée par 60.  Donc si une section est parcourue en 39 secondes, le `carSimulator` fera une pause de 650 millisecondes (39/60).  Un essai libre d'une heure dure donc 1 minutes et une course se termine habituellement en moins de 2 minutes.

## Les différents écrans

### Ecran de démarrage

```
┌──────────────────────────────────────────────────────┐
│ Formula 1 - Manager (by Benjamin/Cyril/Gaylor/Simon) │
└──┬───────────────────────────────────────────────────┴──────────────────────┐
   │ Race #5 - 'Etats-Unis' - 'Miami International Autodrome'                 │
   │ Phase: 'Free Practice 3'                                                 │
   └──────────────────────────────────────────────────────────────────────────┘
   ┌────┬────┬────┬────┬────┬────┬──────┐
   │ F1 │ F2 │ F3 │ Q1 │ Q2 │ Q3 │ RACE │
   └────┴────┴────┴────┴────┴────┴──────┘
 Do you want to start ? (Y/N):
```

C'est le premier écran affiché, il indique le numéro de la course (`5`), le pays (`Etats-Unis`) et le nom du circuit (`Miami International Autodrome`).
On retrouve aussi la prochaine phase qui va s'exécuter (`Free Pratice 3` = essai libre 3).
En dessous de ce cadre, on retrouve la liste de toutes les phases du week-end et la phase qui va être démarrée est affichée avec un fond vert.

### Ecran des essais libres ou qualifications

```
 ┌──────┬────────┬──────────┬──────────┬─────────┬─────────┬─────────┐
 │  #   │ Driver │ status   │ best lap │ best s1 │ best s2 │ best s3 │
 ├──────┼────────┼──────────┼──────────┼─────────┼─────────┼─────────┤
 │  1 = │ 10-GAS │  RUNNING │   76.467 │  25.260 │  26.044 │  25.163 │
 │  2 = │ 22-TSU │ IN STAND │   77.170 │  25.784 │  25.484 │  25.902 │
 │  3 = │ 44-HAM │  RUNNING │   78.000 │  25.303 │  25.470 │  25.072 │
 │  4 = │ 55-SAI │  RUNNING │   79.211 │  27.014 │  26.332 │  25.626 │
 │  5 = │ 18-STR │ IN STAND │   79.770 │  26.739 │  27.555 │  25.405 │
 │  6 = │ 31-OCO │ IN STAND │   80.488 │  26.934 │  26.740 │  26.814 │
 │  7 = │  3-RIC │ IN STAND │   84.872 │  26.551 │  28.254 │  25.754 │
 │  8 = │  1-VER │ IN STAND │   85.834 │  28.568 │  29.060 │  28.206 │
 │  9 ↑ │ 63-RUS │ IN STAND │   89.583 │  29.102 │  29.398 │  30.076 │
 │ 10 ↓ │ 16-LEC │ IN STAND │   91.808 │  30.343 │  30.285 │  31.180 │
 │ 11 = │ 20-MAG │  RUNNING │  100.185 │  31.817 │  34.305 │  33.108 │
 │ 12 = │ 24-GUA │ IN STAND │  101.784 │  33.367 │  33.748 │  33.580 │
 │ 13 = │  4-NOR │  RUNNING │  113.139 │  36.701 │  37.774 │  38.664 │
 │ 14 = │ 81-PIA │ IN STAND │  113.764 │  36.960 │  37.240 │  36.305 │
 │ 15 = │ 77-BOT │ IN STAND │  120.218 │  39.063 │  39.491 │  39.185 │
 │ 16   │ 27-HUL │ IN STAND │  ---.--- │ ---.--- │ ---.--- │ ---.--- │
 │ 17   │ 23-ALB │ IN STAND │  ---.--- │ ---.--- │ ---.--- │ ---.--- │
 │ 18   │  2-SAR │ IN STAND │  ---.--- │ ---.--- │ ---.--- │ ---.--- │
 │ 19   │ 14-ALO │ IN STAND │  ---.--- │ ---.--- │ ---.--- │ ---.--- │
 │ 20   │ 11-PER │ IN STAND │  ---.--- │ ---.--- │ ---.--- │ ---.--- │
 └──────┴────────┴──────────┴──────────┴─────────┴─────────┴─────────┘
```
On retrouve la position des pilotes, avec leur numéro et nom court (en 3 positions), ainsi que le statut et meilleurs temps.  Les meilleurs temps entre tous les pilotes sont affichés en reserve video.
A côté de la position, on retrouve un caractère qui indique le changement de position depuis le dernier rafraichissement (1x/seconde).  4 valeurs sont possibles: 
* `=`: pas de changement
* `↑`: gagne une ou plusieurs places
* `↓`: perd une ou plusieurs places
* ` ` (un blanc): le pilote ne participe pas à la séance car il ne s'est pas qualifié lors de la séance précédente (cela arrive par exemple lors des qualification 2 et 3).
 
Le statut peut être:
* `IN STAND`: le pilote est en attente dans son stand.
* `RUNNING`: il est actuellement occupé à faire un tour de circuit.
* `--OUT--`: le pilote s'est crashé et sa séance d'essai/qualification est interrompue.

Les voitures sont classés en fonction du meilleur temps au tour (en cas d'égalité, on compare la somme des meilleurs temps de chaque section).

### Ecran de sprint ou course
```
┌──────┬────────┬─────────┬─────────────┬─────────┬─────────┬──────────┬─────────┬─────────┬─────────┬─────┐
│  #   │ Driver │ dist.   │  total      │ diff.   │ dif 1st │ best lap │ best s1 │ best s2 │ best s3 │ pit │
├──────┼────────┼─────────┼─────────────┼─────────┼─────────┼──────────┼─────────┼─────────┼─────────┼─────┤
│  1 = │  4-NOR │ 14 (S2) │   19:42.778 │         │         │   76.735 │  25.375 │  25.154 │  25.180 │  0  │
│  2 = │ 44-HAM │ 14 (S2) │   19:43.316 │   0.538 │   0.538 │   75.164 │  25.106 │  25.018 │  25.040 │  0  │
│  3 ↑ │  1-VER │ 14 (S2) │   19:53.423 │  10.107 │  10.645 │   76.230 │  25.556 │  25.033 │  25.242 │  0  │
│  4 ↓ │ 24-GUA │ 14 (S2) │   19:54.914 │   1.491 │  12.136 │   76.854 │  25.006 │  25.050 │  25.011 │  0  │
│  5 = │ 63-RUS │ 14 (S1) │   20:10.985 │  16.071 │  28.207 │   76.005 │  25.340 │  25.039 │  25.257 │  0  │
│  6 = │ 23-ALB │ 13 (S3) │   19:14.719 │   0.847 │  23.084 │   76.928 │  25.864 │  25.279 │  25.203 │  0  │
│  7 = │ 77-BOT │ 12 (S1) │   20:31.153 │  1 lap  │  2 laps │   80.181 │  26.485 │  26.879 │  25.818 │  1  │
│  8 = │ 31-OCO │ 12 (S1) │   20:43.924 │  12.771 │  2 laps │   90.640 │  30.738 │  29.196 │  29.959 │  1  │
│  9 = │  2-SAR │ 11 (S3) │   19:10.950 │   0.109 │  3 laps │   92.989 │  29.920 │  30.269 │  32.623 │  0  │
│ 10 = │ 10-GAS │ 11 (S2) │   19:23.167 │  12.217 │  3 laps │   97.424 │  31.837 │  32.204 │  30.940 │  0  │
│ 11 = │ 20-MAG │ 11 (S2) │   19:25.542 │   2.375 │  3 laps │   98.672 │  32.096 │  32.151 │  32.419 │  0  │
│ 12 = │ 22-TSU │ 11 (S2) │   19:48.642 │  23.100 │  3 laps │   90.666 │  30.247 │  30.813 │  29.606 │  0  │
│ 13 = │ 81-PIA │ 11 (S1) │   20:07.822 │  19.180 │  3 laps │   98.634 │  32.987 │  31.291 │  33.670 │  0  │
│ 14 = │ 16-LEC │ 10 (S3) │   18:58.590 │  10.727 │  4 laps │  107.450 │  35.283 │  35.007 │  36.264 │  0  │
│ 15 = │ 27-HUL │ 10 (S3) │   19:01.505 │   2.915 │  4 laps │   90.874 │  30.142 │  29.252 │  30.454 │  0  │
│ 16 = │ 11-PER │ 10 (S3) │   19:02.907 │   1.402 │  4 laps │  102.572 │  34.869 │  33.182 │  34.521 │  0  │
│ 17 = │  3-RIC │ 10 (S1) │   20:14.865 │  71.958 │  4 laps │  107.516 │  35.788 │  36.197 │  35.531 │  1  │
│ 18 = │ 18-STR │  9 (S3) │   19:05.829 │  12.845 │  5 laps │  119.120 │  38.869 │  39.504 │  40.747 │  0  │
│ 19 = │ 55-SAI │  9 (S3) │   19:06.273 │   0.444 │  5 laps │  113.904 │  37.889 │  37.577 │  38.438 │  0  │
│ 20 = │ 14-ALO │  7 (S2) │   13:03.950 │ --OUT-- │ --OUT-- │  101.303 │  32.208 │  33.613 │  34.529 │  0  │
└──────┴────────┴─────────┴─────────────┴─────────┴─────────┴──────────┴─────────┴─────────┴─────────┴─────┘
```
L'écran est divisé en 11 colonnes:
* `#`: indique la position du pilote dans la course (de 1 à 20), ainsi que le changement depuis le dernier rafraichissement (1x/seconde).
* `Driver`: C'est le numéro et les 3 lettres du pilote
* `dist.`: indique le nombre de tour complet parcouru et entre paranthèse la section en cours.
* `total`: c'est le temps total de course enregistré.  Le temps n'est mis à jour qu'après un passage de fin de section.
* `diff.`: c'est la différence entre le pilote et celui qui le précede.  Si les deux pilotes sont à moins d'un tour d'écart, on affiche la différence en secondes.millisecondes, sinon, on affiche le nombre de tour d'écart.
* `dif 1st`: c'est la différence entre le pilote et le 1er de la course.  Si les deux pilotes sont à moins d'un tour d'écart, on affiche la différence en secondes.millisecondes, sinon, on affiche le nombre de tour d'écart.
* `best lap`: c'est le meilleur temps au tour que le pilote à réaliser.
* `best s1`: c'est le meilleur temps pour la section 1 que le pilote a réalisé.
* `best s2`: c'est le meilleur temps pour la section 2 que le pilote a réalisé.
* `best s3`: c'est le meilleur temps pour la section 3 que le pilote a réalisé.
* `pit`: c'est le nombre d'arrêt au stand (pit stop) que le pilote a effectué depuis le départ.

Dans les colonnes `best lap`, `best s1`, `best s2` et `best s3`, les meilleurs temps entre tous les pilotes sont affichés en reverse video.
Dans les colonnes `diff.` et `dif 1st`, on affiche `--OUT--` si le pilote s'est crashé. 

### Ecran des résultats
```
┌─────────────────┐
│ Pilot's ranking │
├─────┬───────────┴──────────────────┬───────┬──────────┐
│ Pos │ Name                         │ total │ Race won │
├─────┼──────────────────────────────┼───────┼──────────┤
│   1 │ George Russell               │    58 │        1 │
│   2 │ Yuki Tsunoda                 │    52 │        1 │
│   3 │ Lewis Hamilton               │    46 │        1 │
│   4 │ Esteban Ocon                 │    35 │        0 │
│   5 │ Daniel Ricciardo             │    34 │        0 │
│   6 │ Lando Norris                 │    28 │        0 │
│   6 │ Pierre Gasly                 │    28 │        0 │
│   8 │ Lance Stroll                 │    26 │        1 │
│   9 │ Valtteri Bottas              │    26 │        0 │
│  10 │ Sergio Pérez                 │    25 │        1 │
│  11 │ Charles Leclerc              │    25 │        0 │
│  12 │ Alexander Albon              │    22 │        0 │
│  13 │ Zhou Guanyu                  │    20 │        0 │
│  14 │ Max Verstappen               │    16 │        0 │
│  15 │ Carlos Sainz Jr              │    15 │        0 │
│  15 │ Oscar Piastri                │    15 │        0 │
│  15 │ Nico Hülkenberg              │    15 │        0 │
│  18 │ Kevin Magnussen              │    12 │        0 │
│  19 │ Fernando Alonso              │     8 │        1 │
│  20 │ Logan Sargeant               │     5 │        0 │
└─────┴──────────────────────────────┴───────┴──────────┘
```
A la fin de la simulation, le programme affiche les résulats du championnat.
L'écran affiche la place du pilote dans le championnat, le nom complet, le nombre de points gagnés et le nombre de course remporté.
2 pilotes (ou plus) peuvent avoir la même place si ils ont le même nombre de points ET de course gagné (par exemple, la place de 6e dans l'écran ci-dessus).  On peut voir par contre, que le 9e et 10e ont été départagé parce qu'un des deux pilotes avait gagné une course de plus que l'autre. 

## Fonctionnement interne
Les sous-process sont démarrés via des `fork()`.  Au plus fort de l'exécution, on aura 23 processus (le programme "main", le controller, le screenManager et jusqu'à 20 carSimulators).  Ces processus communiquent leurs données via une mémoire partagée.
Afin de garantir un accès cohérent aux données, l'algorithme "Courtois" a été implémenté.  Plusieurs lecteurs peuvent lire les données en même temps, mais quand un process veut mettre les données à jour, il sera le seul process à accéder aux données (accès exclusif).  Tous les accès à la mémoire partagée en mode "concurrent" sont dans des fonctions bien définies

Liste des fonctions qui "lisent" des données
| fonction                                                             | Description |
| :------------------------------------------------------------------- | :----------------- |
| void readSharedMemoryData(void* data, enum SharedMemoryDataType smdt)| Retourne dans la zone `data` l'information demande, cela peut être `CAR_STATS`: les données cumulées des pilotes (total time, distance, best lap, ...), `RUNNING_CARS`: le nombre de pilote encore en course, `CAR_TIME_AND_STATUSES`: les données pour une section (c'est la zone mémoire utilisée par les carSimulator pour envoyer les données aux controller), `RACE_OVER`: un flag qui indique si le course est terminée, ´PROCESSED_FLAGS´: un tableau avec tous les flags "processed" des structures CAR_TIME_AND_STATUS.   |
| int getRunningCars()| Utilise `readSharedMemoryData` pour retourner directement le nombre de pilote encore en course |
| bool isRaceOver()| Utilise `readSharedMemoryData` pour retourner directement le flag RaceOver |

Liste des fonctions qui "écrivent" des données
| fonction                                                              | Description |
| :-------------------------------------------------------------------- | :----------------- |
| void decrementRunningCars()| Décrémente le compteur de pilote encore en course | 
| void setRaceAsOver()| Met à `true` le flag raceOver | 
| void setCarTimeAndStatusAsProcessed(int i)| Met à `true` le flag processed de la zone CarTimeAndStatus n° i | 
| void updateCarStat(CarStat carStat, int i)| Met à jour les données CarStat de la zone n°i | 
| int sendDataToController(int id, CarTimeAndStatus status)| Met à jour les données d'une section pour le controller.  Retourne le nombre de milliseconds "perdues" dans l'opération (temps pour faire l'update de la mémoire partagée) | 



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_PILOT 20   // max number of cars
#define MAX_TRACK 22   // max number of tracks
#define SHM_KEY 0x1234 // uniq key for shared memory

/**
 * This is the formula 1 manager
 * Usage: gcc course.c -o course && ./course
 * What does it do ?
 * # Preparation
 * - read driver.csv and tracks.csv (located next to main executable) to get the driver's names and track's info (name and distance)
 * 		compute number of lap
 * - read championship.txt to determine where we are (race=nn\nphase=nn\n)
 * 		determine next phase (free practice, qualification, sprint or race)
 * # Main
 * - prepare shared memory for inter process communication
 * 		shm between cars and controller (array with seconds.milliseconds, pitstop?, processed?)
 * 		shm between controller and screen manager (array with car total time and distance (how many section passed), best time per section,
 * 												current section time, best lap time, pitstopcount, crashed? )
 * - start controller
 * - start car simulators
 * - start screen manager
 * wait until all childs terminated
 * - save data to file
 *
*/

/* ------------------------------------------
    enum
   ------------------------------------------ */
// status of the car during race or practice
enum CarStatus { WAIT_IN_STAND=0, RUNNING=1, PITSTOP=2, CRASHED=4 };
// all different phases during the week-end
enum RacePhase { START_OF_WEEKEND=0, FREE_PRACTICE_1=1, FREE_PRACTICE_2=2, FREE_PRACTICE_3=3,
				QUALIFICATION_1=4, QUALIFICATION_2=5, QUALIFICATION_3=6,
				SPRINT_QUALIFICATION_1=7, SPRINT_QUALIFICATION_2=8, SPRINT_QUALIFICATION_3=9,
				SPRINT=10, RACE=11, FINISH=99};
// data type readSharedMemoryData can retrieve
enum SharedMemoryDataType { CAR_STATS, RUNNING_CARS, CAR_TIME_AND_STATUSES, RACE_OVER, PROCESSED_FLAGS };

/* ------------------------------------------
    struct
   ------------------------------------------ */
// Struct to store time
typedef struct {
	int seconds;
	int milliseconds;
} CarTime;

// slot for car communication between car simulator and controller
// sectionTime = time taken to do 1/3 of a lap
// carStatus =
//    WAIT_IN_STAND -> if car wait in the stand during qualification/free practice
//    RUNNING       -> when on the track (during race or qualification)
//    PITSTOP       -> during race, if the car did a pit stop during the section run
//    CRASHED       -> if car had a crash
// processed = indicates if the controller already treated the data (if not, the car simulator has to wait before sending new data)
typedef struct {
	CarTime sectionTime;
	enum CarStatus carStatus;
	bool processed;
} CarTimeAndStatus;

// struct using by controller to store car data, used by ScreenManager and after the session to store the data on disk
typedef struct {
	int pilotNumber; // number of the pilot
	CarTime totalTime; // total running time
	int distance; // count the section done (1 = Section 1 is done, 3 = section 1, 2 and 3 done => 1 lap is done, ...)
	CarTime bestSectionTime[3]; // array to store best section time ever
	CarTime currentSectionTime[3]; // array to store last section time (used to calculate lap time)
	CarTime bestLap; // best lap time ever
	int pitStopCount; // count pit stop done
	bool crashed; // is the car crashed ?
	bool inStand; // is the car in the stand ?
} CarStat;

// struct to store score after race or sprint
typedef struct {
	int pilotNumber;
	char name[50];
	int score;
	int raceWon;
} PilotStat;

// shared struct for process communication (SHARED MEMORY)
typedef struct {
	CarTimeAndStatus carTimeAndStatuses[MAX_PILOT];  // we allocate 20 slots for the car to send data to controller
	CarStat carStats[MAX_PILOT]; // 20 slots for all pilot data
	int runningCars; // amount of still running car (when 0 = no car running anymore, all race/qualif ended)
	bool raceOver; // indicate if a pilot reach finish line (all other car must finish their lap and stop)

	sem_t mutex; // semaphore for writers (exclusive access: only 1 writer as access)
	sem_t mutread; // semaphore for readers (shared access: as many readers as needed)
	int readerCount; // count how many reader we have
} SharedMemory;

// Driver's data (data from drivers.csv)
typedef struct {
	int id;
	char name[50];
	char shortName[4]; // 3 letters name
} DriverData;

// Track's data (data from tracks.csv)
typedef struct {
	char country[50];
	char name[50];
	bool sprint; // is this a special week-end with sprint ?
	int length; // length of the track in meters
} TrackData;

// ------------------------------------------
//  Global variables
// ------------------------------------------
DriverData drivers[MAX_PILOT];
TrackData tracks[MAX_TRACK];

// Pointer to shared memory
SharedMemory *sharedMemory = NULL;

// ------------------------------------------
//  Functions
// ------------------------------------------
/**
 * CarTime functions
*/
int compareCarTime(CarTime t1, CarTime t2);
int compareCarStatRace(const void * elem1, const void * elem2);
int compareCarStatQualification(const void * elem1, const void * elem2);
void subCarTime(CarTime *diff, const CarTime t1, const CarTime t2);
void combineCarTime(CarTime *t1, const CarTime t2);
void carTime2String(char* carTimeAsString, CarTime carTime, int digitCount);
void carTime2HMS(char* carTimeAsString, CarTime carTime);

/**
 * General purpose functions
*/
int millisWait(int millis);
char getConfirmation();
const char* racePhaseToShortString(enum RacePhase phase);
char* getDriverName(int id);
int getTrackLap(int trackNumber,enum RacePhase phase);
int getQualifTime(int trackNumber,enum RacePhase phase);
int getScore(int position, enum RacePhase phase);
int getMaxPilotRunning(enum RacePhase phase);
int comparePilotStat(const void * elem1, const void * elem2);

/**
 * File access functions
*/
void readDriverData(DriverData* drivers);
void readTrackData(TrackData* tracks);
void savePhaseResult(int race, enum RacePhase phase, int pilotRunning);
void loadPhaseResult(CarStat* carStats, int race, enum RacePhase phase);
void saveChampionshipResult(int race, enum RacePhase phase);
void loadFinalChampionshipResult(int raceNumber, PilotStat* pilotStats, DriverData* drivers);

/**
 * Screen Manager functions
*/
void getDifferences(char (*differences)[9], CarStat* sorted, bool compareWithFirst, int pilotRunning);
void displayData(const CarStat* carStats, const CarStat* carStatsPrevious, bool race, int pilotRunning);
void screenManager(enum RacePhase phase, int pilotRunning);
void displayLogo();
char* getDriverShortName(int id);

/**
 * Controller functions
*/
void controller(int trackNumber,int phase, int pilotRunning);

/**
 * Car simulator functions
*/
void carSimulator(int id, CarTime delay, int trackNumber,enum RacePhase phase);
void carSimulatorRace(int id, CarTime delay, int maxLap);
void carSimulatorQualification(int id, int maxTime);

/**
 * Shared Memory Functions (implementing "Courtois" algorithm)
*/
void cleanupSharedMemory(int signum);
void readSharedMemoryData(void* data, enum SharedMemoryDataType smdt);
int getRunningCars();
bool isRaceOver();
void decrementRunningCars();
void setRaceAsOver();
void setCarTimeAndStatusAsProcessed(int i);
void updateCarStat(CarStat carStat, int i);
int sendDataToController(int id, CarTimeAndStatus status);

/**
 * main functions
*/
const char* racePhaseToString(enum RacePhase phase);
enum RacePhase getNextPhase(enum RacePhase currentPhase, bool special);
enum RacePhase getPreviousPhase(enum RacePhase currentPhase, bool special);
void displayRanking(int raceNumber);

// ------------------------------------------
//  Functions definitions
// ------------------------------------------
// function to compare 2 car times
int compareCarTime(CarTime t1, CarTime t2) {
	// compare seconds
	if (t1.seconds < t2.seconds) {
		return -1;  // t1 is smaller
	} else if (t1.seconds > t2.seconds) {
		return 1;   // t1 is bigger
	}

	// Seconds are equals, so compare milliseconds
	return t1.milliseconds - t2.milliseconds;
}

// function to compare 2 car records
// will be used to order the car, not crashed car first, then biggest distance and finally totalTime
int compareCarStatRace(const void * elem1, const void * elem2) {
	CarStat cr1 = (CarStat)*(CarStat*)elem1;
	CarStat cr2 = (CarStat)*(CarStat*)elem2;

	// not crashed car comes first
	if (!cr1.crashed && cr2.crashed) {
		return -1;
	} else if (cr1.crashed && !cr2.crashed) {
		return 1;
	}
	// if both crashed or both not crashed, we compare distance and time

	// biggest distance comes first
	if (cr1.distance > cr2.distance) {
		return -1;
	} else if (cr1.distance < cr2.distance) {
		return 1;
	}

	// same distance, better time comes first
	return compareCarTime(cr1.totalTime, cr2.totalTime);
}

// function to compare 2 car records
// will be used to order the car during qualification, compare best lap, if equal, compare addition of the 3 bestSectionTime
int compareCarStatQualification(const void * elem1, const void * elem2) {
	CarStat cr1 = (CarStat)*(CarStat*)elem1;
	CarStat cr2 = (CarStat)*(CarStat*)elem2;
	int compareBestLap = compareCarTime(cr1.bestLap, cr2.bestLap);

	// compare best lap
	if (compareBestLap != 0) {
		return compareBestLap;
	}

	// no best lap or the same, compare best s1, s2, s3
	CarTime ct1, ct2;
	ct1.seconds=0;
	ct1.milliseconds=0;
	ct2.seconds=0;
	ct2.milliseconds=0;
	for (int section=0;section<3;section++) {
		combineCarTime(&ct1,cr1.bestSectionTime[section]);
		combineCarTime(&ct2,cr2.bestSectionTime[section]);
	}

	return compareCarTime(ct1,ct2);
}

void subCarTime(CarTime *diff, const CarTime t1, const CarTime t2) {
	diff->seconds=t1.seconds-t2.seconds;
	diff->milliseconds=t1.milliseconds-t2.milliseconds;
	if (diff->milliseconds<0) {
		diff->milliseconds+=1000;
		diff->seconds--;
	}
}

// add t2 to t1, as t2 could be negative
void combineCarTime(CarTime *t1, const CarTime t2) {
	t1->milliseconds=t1->milliseconds+t2.milliseconds;
	t1->seconds=t1->seconds+t2.seconds;
	if (t1->milliseconds<0) {
		t1->milliseconds+=1000;
		t1->seconds--;
	} else if (t1->milliseconds>=1000) {
		t1->milliseconds-=1000;
		t1->seconds++;
	}
}

// Convert CarTime to String (format nnnn.nnn)
// digitCount could be 3 or 4
void carTime2String(char* carTimeAsString, CarTime carTime, int digitCount) {
	char pszFormat[9]; //"%{n}d.%03d"

	// initialize pszResult
	memset(carTimeAsString,'\0',digitCount+1+3+1);

	// create format string %{n}d%03d
	sprintf(pszFormat,"%%%dd.%%03d",digitCount);

	// Determine MAX value (will be replace by ---)
	// we don't use pow function, don't want to load math library
	int maxValue=10;
	for(int i=0;i<digitCount-1;i++) {
		maxValue*=10;
	}
	maxValue-=1; // 10^1-1 = 9, 10^2-1=99, ...

	// if max value -> replace by ---
	if (carTime.seconds == maxValue && carTime.milliseconds == 999) {
		// set all string to -
		memset(carTimeAsString,'-',digitCount+1+3);
		// add the .
		carTimeAsString[digitCount]='.';
	} else {
		// format CarTime
		sprintf(carTimeAsString,pszFormat,carTime.seconds, carTime.milliseconds);
	}
}

// Convert CarTime to String (format h:mm:ss.nnn)
void carTime2HMS(char* carTimeAsString, CarTime carTime) {
	if (carTime.seconds<60) {
		// SS.mmm
		sprintf(carTimeAsString,"%2d.%03d",carTime.seconds,carTime.milliseconds);
	} else if (carTime.seconds<60*60) {
		// MM:SS.mmm
		sprintf(carTimeAsString,"%2d:%02d.%03d",carTime.seconds/60,carTime.seconds%60,carTime.milliseconds);
	} else {
		// H:MM:SS.mmm
		sprintf(carTimeAsString,"%d:%02d:%02d.%03d",carTime.seconds/3600,(carTime.seconds/60)%60,carTime.seconds%60,carTime.milliseconds);
	}
}

// -----------------------------------------------------------
// function to wait a few milliseconds, return millis
int millisWait(int millis) {
	struct timespec ts;
	ts.tv_sec = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000 * 1000;
	nanosleep(&ts, NULL);

	return millis;
}

char getConfirmation() {
    char input;
    while (1) {
        if (scanf(" %c", &input) == 1) {
            // convert to uppercase
            input = toupper(input);

            if (input == 'Y' || input == 'N') {
                return input;
            }
        }

        // remove input buffer
		while (getchar() != '\n');

        printf("Invalid input. Enter Y or N.\n");
    }
}

// used in filename
const char* racePhaseToShortString(enum RacePhase phase) {
    switch (phase) {
        case START_OF_WEEKEND: return "";
        case FREE_PRACTICE_1: return "F1";
        case FREE_PRACTICE_2: return "F2";
        case FREE_PRACTICE_3: return "F3";
        case QUALIFICATION_1: return "Q1";
        case QUALIFICATION_2: return "Q2";
        case QUALIFICATION_3: return "Q3";
        case SPRINT_QUALIFICATION_1: return "SQ1";
        case SPRINT_QUALIFICATION_2: return "SQ2";
        case SPRINT_QUALIFICATION_3: return "SQ3";
        case SPRINT:          return "sprint";
        case RACE:            return "race";
        default:              return "Unknown_Phase";
    }
}

char* getDriverName(int id) {
	for(int i=0;i<MAX_PILOT;i++) {
		if (drivers[i].id == id) {
			return drivers[i].name;
		}
	}
	return "unknow driver";
}

int getTrackLap(int trackNumber,enum RacePhase phase) {
	if (phase == SPRINT) {
		// Sprint is 100km
		return (100000/tracks[trackNumber].length)+1;
	} else if (phase == RACE) {
		// Race is 300km
		return (300000/tracks[trackNumber].length)+1;
	} else {
		return 0;
	}
}

int getQualifTime(int trackNumber,enum RacePhase phase) {
	switch (phase) {
		case FREE_PRACTICE_1:
		case FREE_PRACTICE_2:
		case FREE_PRACTICE_3: return 60*60*1000; break; // 1h
		case QUALIFICATION_1: return 18*60*1000; break; // 0h18
		case QUALIFICATION_2: return 15*60*1000; break; // 0h15
		case QUALIFICATION_3: return 12*60*1000; break; // 0h12
		case SPRINT_QUALIFICATION_1: return 12*60*1000; break; // 0h12
		case SPRINT_QUALIFICATION_2: return 10*60*1000; break; // 0h10
		case SPRINT_QUALIFICATION_3: return 8*60*1000; break; // 0h08
		default: return 0;
	}
}

int getScore(int position, enum RacePhase phase) {
	if (phase == SPRINT) {
		if (position < 8) {
			return 8 - position;
		} else {
			return 0;
		}
	} else {
		switch (position) {
			case 0: return 25;
			case 1: return 20;
			case 2: return 15;
			case 3: return 10;
			case 4: return 8;
			case 5: return 6;
			case 6: return 5;
			case 7: return 3;
			case 8: return 2;
			case 9: return 1;
			default: return 0;
		}
	}
}

int getMaxPilotRunning(enum RacePhase phase) {
	switch (phase) {
		case FREE_PRACTICE_1:
		case FREE_PRACTICE_2:
		case FREE_PRACTICE_3:
		case QUALIFICATION_1: return MAX_PILOT;
		case QUALIFICATION_2: return 15;
		case QUALIFICATION_3: return 10;
		case SPRINT_QUALIFICATION_1: return MAX_PILOT;
		case SPRINT_QUALIFICATION_2: return 15;
		case SPRINT_QUALIFICATION_3: return 10;
		case SPRINT: return MAX_PILOT;
		case RACE: return MAX_PILOT;
		default: return MAX_PILOT;
	}
}

// compare pilot stat ranking
int comparePilotStat(const void * elem1, const void * elem2) {
	PilotStat pilot1 = (PilotStat)*(PilotStat*)elem1;
	PilotStat pilot2 = (PilotStat)*(PilotStat*)elem2;

	// higher score first
	if (pilot1.score != pilot2.score) {
		return pilot2.score - pilot1.score;
	}

	// same score => compare race won
	return pilot2.raceWon - pilot1.raceWon;
}

// --------------------------------------------------------------------
// read drivers.csv and store data into global variable drivers
void readDriverData(DriverData *drivers) {
	FILE *file = fopen("drivers.csv", "r");

    int driverCount = 0;
    if (file == NULL) {
		perror("Unable to read drivers");
		exit(1);
	} else {
		char line[100]; // Buffer
		while (fgets(line, sizeof(line), file) && driverCount < MAX_PILOT) {
			// Read ID and Name (max 49 character long)
			sscanf(line, "%d;%3[^;];%49[^\n]", &drivers[driverCount].id, drivers[driverCount].shortName, drivers[driverCount].name);
			// increment counter
			driverCount++;
		}

		fclose(file);
	}
}

// read tracks.csv and store data into global variable tracks
void readTrackData(TrackData* tracks) {
    FILE *file = fopen("tracks.csv", "r");

    int trackCount = 0;
    if (file == NULL) {
		perror("Unable to read tracks.csv");
		exit(1);
	} else {
		char line[200]; // Buffer
		while (fgets(line, sizeof(line), file) && trackCount < MAX_TRACK) {
			char weekendType[20];
			// country, name and length
			sscanf(line, "%49[^;];%49[^;];%49[^;];%d", tracks[trackCount].country, tracks[trackCount].name, weekendType, &tracks[trackCount].length);

			// convert special/normal to boolean
			if (strcmp(weekendType,"sprint") == 0) {
				tracks[trackCount].sprint = true;
			} else {
				tracks[trackCount].sprint = false;
			}
			// increment counter
			trackCount++;
		}

		fclose(file);
	}
}

void savePhaseResult(int race, enum RacePhase phase, int pilotRunning) {
	// determine filename
	char filename[25];
	sprintf(filename,"race_%02d_%s.csv",race+1,racePhaseToShortString(phase));
	FILE* file = fopen(filename,"w");
	if (!file) {
		// error while creating the file
		perror("Unable to save phase result.csv");
		exit(1);
	} else {
		// Sort data
		CarStat carStats[MAX_PILOT] = {0};
		memcpy(carStats, &sharedMemory->carStats, sizeof(CarStat)*MAX_PILOT);
		if (phase == SPRINT || phase == RACE) {
			qsort(carStats,pilotRunning,sizeof(CarStat),compareCarStatRace);
		} else {
			qsort(carStats,pilotRunning,sizeof(CarStat),compareCarStatQualification);
		}

		// Save data
		for (int pilot=0;pilot<MAX_PILOT;pilot++) {
			// save pilot id, bestLap, bestS1, bestS2, bestS3
			fprintf(file,"%d;%d.%d;%d.%d;%d.%d;%d.%d\n",
				carStats[pilot].pilotNumber,
				carStats[pilot].bestLap.seconds, carStats[pilot].bestLap.milliseconds,
				carStats[pilot].bestSectionTime[0].seconds, carStats[pilot].bestSectionTime[0].milliseconds,
				carStats[pilot].bestSectionTime[1].seconds, carStats[pilot].bestSectionTime[1].milliseconds,
				carStats[pilot].bestSectionTime[2].seconds, carStats[pilot].bestSectionTime[2].milliseconds
			);
		}
	}
	fclose(file);
}

void loadPhaseResult(CarStat* carStats, int race, enum RacePhase phase) {
	// determine filename
	char filename[25];
	sprintf(filename,"race_%02d_%s.csv",race+1,racePhaseToShortString(phase));

	FILE* file = fopen(filename,"r");
	if (!file) {
		// error while opening the file
		perror("Unable to read phase result.csv");
		exit(1);
	}

	for (int pilot=0;pilot<MAX_PILOT;pilot++) {
		// read pilot id, bestLap, bestS1, bestS2, bestS3
		if (!fscanf(file, "%d;%d.%d;%d.%d;%d.%d;%d.%d\n",
				&carStats[pilot].pilotNumber,
				&carStats[pilot].bestLap.seconds, &carStats[pilot].bestLap.milliseconds,
				&carStats[pilot].bestSectionTime[0].seconds, &carStats[pilot].bestSectionTime[0].milliseconds,
				&carStats[pilot].bestSectionTime[1].seconds, &carStats[pilot].bestSectionTime[1].milliseconds,
				&carStats[pilot].bestSectionTime[2].seconds, &carStats[pilot].bestSectionTime[2].milliseconds
			)) {
			// file content is invalid
			perror("unable to read result file");
			exit(1);
		}
		carStats[pilot].inStand = true;
	}
	fclose(file);
}

void saveChampionshipResult(int race, enum RacePhase phase) {
	// determine filename
	char filename[25];
	sprintf(filename,"race_%02d_%s_ranking.csv",race+1,racePhaseToShortString(phase));

	FILE* file = fopen(filename,"w");
	if (!file) {
		// error while creating the file
		perror("Unable to save championship ranking.csv");
		exit(1);
	} else {
		// Sort race data
		CarStat carStats[MAX_PILOT];
		memcpy(carStats, sharedMemory->carStats, sizeof(CarStat)*MAX_PILOT);
		qsort(carStats,MAX_PILOT,sizeof(CarStat),compareCarStatRace);

		// Save data
		for (int position=0;position<MAX_PILOT;position++) {
			// save pilot id, point for the race
			fprintf(file,"%d;%d\n",	carStats[position].pilotNumber, getScore(position,phase));
		}
	}
	fclose(file);
}

void loadFinalChampionshipResult(int raceNumber, PilotStat* pilotStats, DriverData* drivers) {
	// read all ranking files and compute pilot result
	// Initialize results
	for (int i=0;i<MAX_PILOT;i++) {
		pilotStats[i].pilotNumber=drivers[i].id;
		strcpy(pilotStats[i].name, drivers[i].name);
		pilotStats[i].score=0;
		pilotStats[i].raceWon=0;
	}

	// loop on races and sprints
	enum RacePhase racePhases[2];
	racePhases[0]=SPRINT;
	racePhases[1]=RACE;
	for (int race=0;race<=raceNumber;race++) {
		for (int phase=0;phase<2;phase++) {
			int pilotNumber=0;
			int score=0;

			char filename[25];
			// create filename
			sprintf(filename,"race_%02d_%s_ranking.csv",race+1,racePhaseToShortString(racePhases[phase]));
			// check if file exist:
			if (access(filename, F_OK) != 0) {
				// file does not exist, skip
				continue;
			}

			FILE* file = fopen(filename,"r");
			if (!file) {
				perror("Unable to open ranking file");
				exit(1);
			}

			// read result
			for (int i=0;i<MAX_PILOT;i++) {
				// read ranking line
				fscanf(file, "%d;%d\n",&pilotNumber,&score);

				// search pilot in the list and update score
				for (int j=0;j<MAX_PILOT;j++) {
					if (pilotStats[j].pilotNumber == pilotNumber) {
						pilotStats[j].score+=score;
						if (i==0) {
							// pilot won the race
							pilotStats[j].raceWon++;
						}
						break;
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------
// Compute the difference between 2 pilots
// either with first (compareWithFirst==true) or previous
// pilotRunning = number of pilot running (during Q2/Q3 not all pilot are running)
// 1. Same distance or 3 section diff = difference of totalTime
// 2. 1 or 2 section diff => add last section and compare totalTime
// 3. at least 4 sections diff => compare lap
void getDifferences(char (*differences)[9], CarStat* sorted, bool compareWithFirst, int pilotRunning) {
	CarTime ctDiff;

	strcpy(differences[0],""); // 1st pilot has no difference with previous :-)
	if (sorted[0].distance >= 3) { // only compare if at least one lap is done by first pilot !
		for(int i=1;i<pilotRunning;i++) {
			int indexCompare = compareWithFirst ? 0 : i-1;
			// Check if pilots are in the same lap
			if (sorted[indexCompare].distance/3 == sorted[i].distance/3) {
				// in the same lap => compare totaltime
				if (compareCarTime(sorted[indexCompare].totalTime,sorted[i].totalTime)>=0) {
					// 1st just overtake 2nd car (so totalTime is not relevant)
					strcpy(differences[i],"over");
				} else {
					subCarTime(&ctDiff,sorted[i].totalTime,sorted[indexCompare].totalTime);
					carTime2String(differences[i],ctDiff,3);
				}
			} else if ((sorted[indexCompare].distance-sorted[i].distance)<3) {
				// not the same lap, add last section time between to compare same timing
				CarTime estimatedTotalTime;
				estimatedTotalTime.seconds=sorted[i].totalTime.seconds;
				estimatedTotalTime.milliseconds=sorted[i].totalTime.milliseconds;
				combineCarTime(&estimatedTotalTime,sorted[i].currentSectionTime[0]);
				combineCarTime(&estimatedTotalTime,sorted[i].currentSectionTime[1]);

				// compare to see if 2nd car is not about to overtake 1st car ?
				if (compareCarTime(sorted[indexCompare].totalTime,estimatedTotalTime)>=0) {
					// 2nd car is about to overtake 1st car, create fictive diff
					ctDiff.seconds=0;
					ctDiff.milliseconds=(sorted[indexCompare].totalTime.milliseconds+estimatedTotalTime.milliseconds) % 1000;
				} else {
					// calculate difference
					subCarTime(&ctDiff,estimatedTotalTime,sorted[indexCompare].totalTime);
				}
				carTime2String(differences[i],ctDiff,3);
			} else {
				// more than 3 sections diff => compute lap difference
				int diffLap=(sorted[indexCompare].distance/3)-(sorted[i].distance/3);
				sprintf(differences[i],"%d lap%s",diffLap,(diffLap>1)?"s":" ");
			}
		}
	}
	// no difference for pilot not running
	for (int i=pilotRunning;i<MAX_PILOT;i++) {
		strcpy(differences[i],"");
	}
}

// function to display on the screen the carStats
// race = true we are in sprint or race
// pilotRunning = pilot running (in Q2/Q3 not all pilots are running)
void displayData(const CarStat* carStats, const CarStat* carStatsPrevious, bool race, int pilotRunning) {
	// create a copy of the car records (qsort will modify the order)
	CarStat sorted[MAX_PILOT] = {0};
	CarStat sortedPrevious[MAX_PILOT] = {0};
	memcpy(sorted,carStats,sizeof(CarStat)*MAX_PILOT);
	memcpy(sortedPrevious,carStatsPrevious,sizeof(CarStat)*MAX_PILOT);
	// sort the car records
	if (race) {
		qsort(sorted,pilotRunning,sizeof(CarStat),compareCarStatRace);
		qsort(sortedPrevious,pilotRunning,sizeof(CarStat),compareCarStatRace);
	} else {
		qsort(sorted,pilotRunning,sizeof(CarStat),compareCarStatQualification);
		qsort(sortedPrevious,pilotRunning,sizeof(CarStat),compareCarStatQualification);
	}

	// cars not running wait in stand (during Q2, Q3, ...)
	for (int i=pilotRunning;i<MAX_PILOT;i++){
		sorted[i].inStand = true;
	}

	// determine who has best lap and best section 1, 2 or 3
	int indexBestLap = -1;
	int indexBestSection[3] = {-1};
	CarTime ctBestLap;
	CarTime ctBestSection[3];
	ctBestLap.seconds=999;
	ctBestSection[0].seconds=99;
	ctBestSection[1].seconds=99;
	ctBestSection[2].seconds=99;

	for (int i=0;i<pilotRunning;i++) {
		if (compareCarTime(sorted[i].bestLap,ctBestLap) < 0) {
			ctBestLap.seconds=sorted[i].bestLap.seconds;
			ctBestLap.milliseconds=sorted[i].bestLap.milliseconds;
			indexBestLap=i;
		}

		for (int j=0;j<3;j++) {
			if (compareCarTime(sorted[i].bestSectionTime[j],ctBestSection[j]) < 0) {
				ctBestSection[j].seconds=sorted[i].bestSectionTime[j].seconds;
				ctBestSection[j].milliseconds=sorted[i].bestSectionTime[j].milliseconds;
				indexBestSection[j]=i;
			}
		}
	}

	// Determine if pilot's position changed
	//  could be =, ↑ or ↓
	char* posUpd[MAX_PILOT];
	for(int i=0;i<pilotRunning;i++) {
		posUpd[i]="\033[32m↑\033[0m"; // if pilot is not discovered at the same position or below, it must be better ;-)
		if (sorted[i].pilotNumber == sortedPrevious[i].pilotNumber) {
			posUpd[i]="="; // pilot is at same position
		} else {
			for(int j=0;j<i;j++) {
				if (sorted[i].pilotNumber == sortedPrevious[j].pilotNumber) {
					posUpd[i]="\033[31m↓\033[0m"; // pilot has lost position
				}
			}
		}
	}
	// no update for pilot not running
	for (int i=pilotRunning;i<MAX_PILOT;i++) {
		posUpd[i]=" "; // if pilot is not discovered at the same position or below, it must be better ;-)
	}

	// Determine difference between 2 pilots and difference with 1st
	char differences[MAX_PILOT][9] = {0};
	char differences1st[MAX_PILOT][9] = {0};
	getDifferences(differences, sorted, false, pilotRunning);
	getDifferences(differences1st, sorted, true, pilotRunning);

	// display if anyone is CRASHED
	bool crashedIndex[MAX_PILOT]={false};
	for(int i=0;i<MAX_PILOT;i++) {
		if (sorted[i].crashed) {
			strcpy(differences[i],"--OUT--");
			strcpy(differences1st[i],"--OUT--");
			crashedIndex[i]=true;
		}
	}

	// get screen size (could be used to put the display in the top right corner of the screen)
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	// print using ANSI ESCAPE sequence
	// \033[s = save current cursor position
	// \033[u = restore cursor to previously saved position
	// \033[{line};{column}H = move cursor to position {col}, {column}

	printf("\033[s");
	if (race) {
		printf("\033[%d;%dH%s", 1, w.ws_col-108, "┌──────┬────────┬─────────┬─────────────┬─────────┬─────────┬──────────┬─────────┬─────────┬─────────┬─────┐");
		printf("\033[%d;%dH%s", 2, w.ws_col-108, "│  #   │ Driver │ dist.   │  total      │ diff.   │ dif 1st │ best lap │ best s1 │ best s2 │ best s3 │ pit │");
		printf("\033[%d;%dH%s", 3, w.ws_col-108, "├──────┼────────┼─────────┼─────────────┼─────────┼─────────┼──────────┼─────────┼─────────┼─────────┼─────┤");
		//                                       "│ nn = │ nn-XXX │ nn (Sn) │ h:mm:ss.nnn │ XXX.XXX │ XXX.XXX │  nnn.nnn │ nnn.nnn │ nnn.nnn │ nnn.nnn │  n  │
	} else {
		printf("\033[%d;%dH%s", 1, w.ws_col-68,  "┌──────┬────────┬──────────┬──────────┬─────────┬─────────┬─────────┐");
		printf("\033[%d;%dH%s", 2, w.ws_col-68,  "│  #   │ Driver │ status   │ best lap │ best s1 │ best s2 │ best s3 │");
		printf("\033[%d;%dH%s", 3, w.ws_col-68,  "├──────┼────────┼──────────┼──────────┼─────────┼─────────┼─────────┤");
		//                                       "│ nn = │ nn-XXX │ IN STAND │  nnn.nnn │ nnn.nnn │ nnn.nnn │ nnn.nnn │
	}

	for (int i=0;i<MAX_PILOT;i++) {
		char bestLap[8],bestS1[8],bestS2[8],bestS3[8];
		// convert CarTimes to Strings
		carTime2String(bestLap,sorted[i].bestLap,3);
		carTime2String(bestS1,sorted[i].bestSectionTime[0],3);
		carTime2String(bestS2,sorted[i].bestSectionTime[1],3);
		carTime2String(bestS3,sorted[i].bestSectionTime[2],3);

		if (race) {
			char totalTime[12];
			carTime2HMS(totalTime,sorted[i].totalTime);

			printf("\033[%d;%dH│ %2d %s │ %2d-%s │ %2d (S%d) │ %11s │ %s%7s\033[0m │ %s%7s\033[0m │  %s%7s\033[27m │ %s%7s\033[27m │ %s%7s\033[27m │ %s%7s\033[27m │  %d  │",
				4+i, w.ws_col-108, i+1, posUpd[i],
				sorted[i].pilotNumber, getDriverShortName(sorted[i].pilotNumber), sorted[i].distance / 3, (sorted[i].distance % 3)+1,
				totalTime,
				crashedIndex[i] ? "\033[31m":"", differences[i], crashedIndex[i] ? "\033[31m":"", differences1st[i],
				i==indexBestLap && ctBestLap.seconds != 9999 ? "\033[7m" : "", bestLap,
				i==indexBestSection[0] && ctBestSection[0].seconds != 99 ? "\033[7m" : "", bestS1,
				i==indexBestSection[1] && ctBestSection[1].seconds != 99 ? "\033[7m" : "", bestS2,
				i==indexBestSection[2] && ctBestSection[2].seconds != 99 ? "\033[7m" : "", bestS3,
				sorted[i].pitStopCount
			);
		} else {
			printf("\033[%d;%dH│ %2d %s │ %2d-%s │ %s%8s\033[0m │  %s%7s\033[27m │ %s%7s\033[27m │ %s%7s\033[27m │ %s%7s\033[27m │",
				4+i, w.ws_col-68,
				i+1, posUpd[i],
				sorted[i].pilotNumber, getDriverShortName(sorted[i].pilotNumber),
				crashedIndex[i] ? "\033[31m": sorted[i].inStand ? "\033[36m" : "\033[32m",
				crashedIndex[i] ? "--OUT--": sorted[i].inStand ? "IN STAND" : "RUNNING",
				i==indexBestLap && ctBestLap.seconds != 9999 ? "\033[7m" : "", bestLap,
				i==indexBestSection[0] && ctBestSection[0].seconds != 99 ? "\033[7m" : "", bestS1,
				i==indexBestSection[1] && ctBestSection[1].seconds != 99 ? "\033[7m" : "", bestS2,
				i==indexBestSection[2] && ctBestSection[2].seconds != 99 ? "\033[7m" : "", bestS3
			);
		}
	}
	if (race) {
		printf("\033[%d;%dH%s", 4+MAX_PILOT, w.ws_col-108, "└──────┴────────┴─────────┴─────────────┴─────────┴─────────┴──────────┴─────────┴─────────┴─────────┴─────┘");
	} else {
		printf("\033[%d;%dH%s", 4+MAX_PILOT, w.ws_col-68, "└──────┴────────┴──────────┴──────────┴─────────┴─────────┴─────────┘");
	}
	printf("\033[u");
	fflush(stdout);
}

void screenManager(enum RacePhase phase, int pilotRunning) {
	CarStat carStatsPrevious[MAX_PILOT];
	CarStat carStats[MAX_PILOT];

	// Get at least once the data (for comparison)
	readSharedMemoryData(carStatsPrevious, CAR_STATS);

	// race or qualification ?
	bool race = (phase == RACE) || (phase == SPRINT);

	while (getRunningCars() > 0) {
		// read data
		readSharedMemoryData(carStats, CAR_STATS);
		// update display
		displayData(carStats, carStatsPrevious, race, pilotRunning);
		// copy carStats to carStatsPrevious
		memcpy(carStatsPrevious, carStats, sizeof(CarStat)*MAX_PILOT);
		// wait 1 seconds
		millisWait(1000);
	}

	// No more car running, do a last update and display
	// read data and display them
	millisWait(1000);
	readSharedMemoryData(carStats, CAR_STATS);
	displayData(carStats, carStatsPrevious, race, pilotRunning);
}

void displayLogo() {
	// get screen size
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	const char *CAR_LOGO[]
	= {
	"                     __                                 ",
    "               _.--\"\"  |                                ",
    ".----.     _.-'   |/\\| |.--.                            ",
    "|    |__.-'   _________|  |_)  _______________          ",
    "|  .-\"\"-.\"\"\"\"\" ___,    `----'\"))   __   .-\"\"-.\"\"\"\"--._  ",
    "'-' ,--. `    |   |   .---.       |:.| ' ,--. `      _`.",
    " ( (    ) ) __|   |__ \\\\|// _..--  \\/ ( (    ) )--._\".-.",
    "  . `--' ;\\__________________..--------. `--' ;--------'",
    "   `-..-'                               `-..-'          "};

	// \033[s = save current cursor position
	// \033[u = restore cursor to previously saved position
	// \033[{line};{column}H = move cursor to position {col}, {column}
	// save cursor position
	printf("\033[s\033[2J]");
	// First we made the car appear 1 "col" at a time
	for (int i=56;i>0;i--) {
		for(int l=0;l<9;l++) {
			printf("\033[%d;0H%s",l,CAR_LOGO[l]+i);
		}
		printf("\033[9;0H%*s",w.ws_col,"");
		fflush(stdout);
		millisWait(50);
	}
	// Them the car moved till the end
	for (int i=0;i<w.ws_col+1;i++) {
		for(int l=0;l<9;l++) {
			printf("\033[%d;0H%*s%s",l,i,"",CAR_LOGO[l]);
		}
		printf("\033[9;0H%*s",w.ws_col,"");
		fflush(stdout);
		millisWait(i<45 ? 50-i : 5);
	}

	// restore cursor position
	printf("\033[u");
}

char* getDriverShortName(int id) {
	for(int i=0;i<MAX_PILOT;i++) {
		if (drivers[i].id == id) {
			return drivers[i].shortName;
		}
	}
	return "???";
}

// ---------------------------------------------------------------------------------
// Controller main function
void controller(int trackNumber, int phase, int pilotRunning) {
	// determine number of lap
	int maxLap=getTrackLap(trackNumber, phase);
	CarTimeAndStatus carTimeAndStatuses[MAX_PILOT];
	CarStat carStats[MAX_PILOT];

	bool controllerStop = false;
	// infinite loop (will be stopped when all cars are stopped)
	while (!controllerStop) {
		// check if still car simulator running
		controllerStop = (getRunningCars() == 0);

		// read CarTimeAndStatuses
		readSharedMemoryData(carTimeAndStatuses,CAR_TIME_AND_STATUSES);

		// Check if any data from car simulator to proceed
		bool processPending=false;
		for (int i = 0; i < pilotRunning; i++) {
			if (!carTimeAndStatuses[i].processed) {
				processPending=true;
				break;
			}
		}

		// true only if no car running and no data to proceed (to make sure to treat all data)
		controllerStop &= !processPending;

		if (!processPending) {
			// Nothing to do, will wait .1 second and check again
			millisWait(100);
			continue;
		}

		// read CarStat (we have update to do)
		readSharedMemoryData(carStats,CAR_STATS);

		// Check all records
		for (int i = 0; i < pilotRunning; i++) {
			// processed == 0 => data still to be processed
			if (carTimeAndStatuses[i].processed == false) {
				if (carTimeAndStatuses[i].carStatus == CRASHED) {
					// car crashed => no time to proceed
					carStats[i].crashed=true;
				} else if (carTimeAndStatuses[i].carStatus == WAIT_IN_STAND) {
					// car in stand => no time to proceed
					carStats[i].inStand=true;
				} else {
					// car running (so no more in stand)
					carStats[i].inStand=false;

					// did a pit stop ?
					if (carTimeAndStatuses[i].carStatus == PITSTOP) {
						carStats[i].pitStopCount++;
					}

					// determine the section
					// 0 -> 1st section, 1 = 2nd section, 2 = 3rd section
					int sectionNumber = (carStats[i].distance % 3);

					// increment distance (counting number of section done, number of lap distance/3)
					carStats[i].distance++;

					// save section timing
					carStats[i].currentSectionTime[sectionNumber].seconds=carTimeAndStatuses[i].sectionTime.seconds;
					carStats[i].currentSectionTime[sectionNumber].milliseconds=carTimeAndStatuses[i].sectionTime.milliseconds;

					// Compare with best section
					if (compareCarTime(carStats[i].currentSectionTime[sectionNumber], carStats[i].bestSectionTime[sectionNumber]) < 0) {
						// new best section time
						carStats[i].bestSectionTime[sectionNumber].seconds = carStats[i].currentSectionTime[sectionNumber].seconds;
						carStats[i].bestSectionTime[sectionNumber].milliseconds = carStats[i].currentSectionTime[sectionNumber].milliseconds;
					}

					// if 3rd section, compute lap time
					if (sectionNumber==2) {
						// lapTime = S1 + S2 + S3
						CarTime lapTime;
						lapTime.seconds = carStats[i].currentSectionTime[0].seconds;
						lapTime.milliseconds = carStats[i].currentSectionTime[0].milliseconds;
						combineCarTime(&lapTime,carStats[i].currentSectionTime[1]);
						combineCarTime(&lapTime,carStats[i].currentSectionTime[2]);

						// check if new best lapTime
						if (compareCarTime(lapTime, carStats[i].bestLap) < 0) {
							carStats[i].bestLap.seconds = lapTime.seconds;
							carStats[i].bestLap.milliseconds = lapTime.milliseconds;
						}

						// Update total time
						combineCarTime(&(carStats[i].totalTime), lapTime);
					}

					// check if race is over
					if (phase == SPRINT || phase == RACE) {
						if (carStats[i].distance/3 == maxLap) {
							// race is over, all other simulator must stop
							setRaceAsOver();
						}
					}
				}

				// Mark CarTimeAndStatus as processed
				setCarTimeAndStatusAsProcessed(i);

				// update CarStat (for screen Manager)
				updateCarStat(carStats[i],i);
			}
		}
	}
}

// ---------------------------------------------------------------
/**
 * Car Simulator functions
 * carSimulator: main car simulator function, determine what to do (qualif, race, ...)
 * carSimulatorRace: do a race or sprint
 * carSimulatorQualification: do qualification or free practise
*/

// id = car number
// delay = time lost due to start of the race (1st car has no delay, 2nd .5s delay, ...)
// trackNumber = to determine which track we do
// racePhase = determine phase (qualification, sprint, race, ...)
void carSimulator(int id, CarTime delay, int trackNumber, enum RacePhase phase) {
	// initialize randomizer with time+id, so that all car are different :-)
	srand(time(NULL)+id);

	// determine what to do
	switch (phase) {
		case FREE_PRACTICE_1:
		case FREE_PRACTICE_2:
		case FREE_PRACTICE_3:
		case QUALIFICATION_1:
		case QUALIFICATION_2:
		case QUALIFICATION_3:
		case SPRINT_QUALIFICATION_1:
		case SPRINT_QUALIFICATION_2:
		case SPRINT_QUALIFICATION_3:
			int time=getQualifTime(trackNumber, phase);
			carSimulatorQualification(id, time);
			break;
		case SPRINT:
		case RACE:
			int maxLap=getTrackLap(trackNumber, phase);
			carSimulatorRace(id, delay, maxLap);
			break;
	}

	// the car finished practice, qualification, sprint or race
	decrementRunningCars();
}

// -----------------------------------------
// carSimulatorRace
// id = car number
// delay = time lost due to start of the race (1st car has no delay, 2nd .5s delay, ...)
// maxLap = how many lap to to end the race
void carSimulatorRace(int id, CarTime delay, int maxLap) {
	// initialize randomizer with time+id, so that all car are different :-)
	srand(time(NULL)+(id*100));

	// determine very 1st section time
	CarTime sectionTime;
	sectionTime.seconds = 25 + (rand() % 10); // between 25 and 35 seconds (we will add delay)
	sectionTime.milliseconds = rand() % 1000;

	// add delay (time lost on starting grid), between 0 and 9,5 seconds
	combineCarTime(&sectionTime,delay);

	// determine pit stop strategy
	int pitStopLap[3];
	if (maxLap < 35 || (rand() % 2) == 0) {
		// 2 stops
		pitStopLap[0]=maxLap/3+(rand()%11)-5;
		pitStopLap[1]=(maxLap/3)*2+(rand()%11)-5;
		pitStopLap[2]=9999; // no 3rd pit stop
	} else {
		// 3 stops
		pitStopLap[0]=maxLap/4+(rand()%11)-5;
		pitStopLap[1]=maxLap/2+(rand()%11)-5;
		pitStopLap[2]=(maxLap/4)*3+(rand()%11)-5;
	}

	// loop for the number of lap * 3 (number of section)
	bool running = true;
	bool crashed = false;
	bool pitStop = false;
	for (int i = 0; i < maxLap*3 && running; i++) {
		// check if pit stop
		if ((i % 3 == 2) && ((i/3)==pitStopLap[0] || (i/3)==pitStopLap[1] || (i/3)==pitStopLap[2])) {
			CarTime pitStopTime;
			pitStopTime.seconds = 15 + (rand() % 5);
			pitStopTime.milliseconds = rand() % 1000;
			combineCarTime(&sectionTime,pitStopTime);
			pitStop = true;
		} else {
			pitStop = false;
		}

		// check if end of race (someone already reached finish line)
		if (i % 3 == 2) {
			if (isRaceOver()) {
				running = false;
			}
		}

		// check if car has not crashed
		if ((rand() % 3000) == 0) {
			crashed=true;
			running=false;
		}

		// in case we are not able to immediatly update data, we will wait a few milliseconds, keep track of those waits
		CarTimeAndStatus carTimeStatus;
		long alreadyWait;

		if (crashed) {
			carTimeStatus.carStatus = CRASHED;
		} else {
			carTimeStatus.sectionTime.seconds = sectionTime.seconds;
			carTimeStatus.sectionTime.milliseconds = sectionTime.milliseconds;
			carTimeStatus.carStatus = pitStop ? PITSTOP : RUNNING;
		}
		alreadyWait = sendDataToController(id,carTimeStatus);

		if (!crashed) {
			// simulate time on track
			millisWait(((1000L * sectionTime.seconds + sectionTime.milliseconds) / 60L) - alreadyWait);

			// determine next section time, between 3 seconds more or less than current section time
			// note: may not be less than 25 or more then 45
			CarTime deltaCarTime;

			// add between -2.999 and 2.999
			deltaCarTime.seconds = (rand() % 5) - 2;
			deltaCarTime.milliseconds = (rand() % 1998) - 999;
			combineCarTime(&sectionTime,deltaCarTime);

			// Fix if more than 45 or less than 25 (between 25.000 and 44.999)
			if (sectionTime.seconds >= 45) {
				sectionTime.seconds = 44;
			} else if (sectionTime.seconds < 25) {
				sectionTime.seconds=25;
			}
		}
	}
}

// id = car number
// maxTime = how many time, tha car may try to run
void carSimulatorQualification(int id, int maxTime) {
	// initialize randomizer with time+id, so that all car are different :-)
	srand(time(NULL)+id);

	// determine base section time
	CarTimeAndStatus carTimeStatus;
	carTimeStatus.sectionTime.seconds = 25 + (rand() % 20); // between 25 and 44 seconds
	carTimeStatus.sectionTime.milliseconds = rand() % 1000;

	// loop as long as time is not reached
	int sessionTime=0;
	while (sessionTime < maxTime) {
		// in case we are not able to immediatly update data, we will wait a few milliseconds, keep track of those waits
		long alreadyWait;

		// 1. car waits in stand (1 to 7 minutes).  Min qualif time is 8 minutes !
		int waitInStand = (((rand() % 7) + 1) * 60) * 1000;
		sessionTime += waitInStand;
		carTimeStatus.carStatus = WAIT_IN_STAND;
		alreadyWait = sendDataToController(id,carTimeStatus);
		if (sessionTime > maxTime) {
			// no time left to do a final lap, stop carSimulator immediatly
			break;
		}
		// simulate time in stand
		millisWait((waitInStand / 60L) - alreadyWait);

		// 2. do a qualification lap
		// 2a. check if car has not crashed
		if (rand() % 1000 == 0) {
			carTimeStatus.carStatus = CRASHED;
			sendDataToController(id,carTimeStatus);
			break; // car crashed, stop carSimulator
		}

		// 2b. do the lap
		for (int section=0;section<3;section++) {
			CarTime updateTime;
			updateTime.seconds=(rand() % 5) - 2; // between -2 and 2
			updateTime.milliseconds=(rand() % 1999) - 999; // between -999 and 999
			combineCarTime(&(carTimeStatus.sectionTime), updateTime);

			// not less than 25s and more than 45s
			if (carTimeStatus.sectionTime.seconds<25) {
				carTimeStatus.sectionTime.seconds=25;
			} else if (carTimeStatus.sectionTime.seconds>44) {
				carTimeStatus.sectionTime.seconds=44;
			}

			// add time to session
			sessionTime += carTimeStatus.sectionTime.seconds * 1000 + carTimeStatus.sectionTime.milliseconds;
			carTimeStatus.carStatus = RUNNING;
			alreadyWait = sendDataToController(id,carTimeStatus);
			// simulate time on track
			millisWait(((1000L * carTimeStatus.sectionTime.seconds + carTimeStatus.sectionTime.milliseconds) / 60L) - alreadyWait);
		}
	}

	// Qualification is over -> return to stand
	if (carTimeStatus.carStatus != CRASHED) {
		carTimeStatus.carStatus = WAIT_IN_STAND;
		sendDataToController(id,carTimeStatus);
	}
}

// -------------------------------------------------------------
// Shared memory Clean-up function
void cleanupSharedMemory(int signum) {
	if (sharedMemory) {
		// destroy semaphores
		sem_destroy(&sharedMemory->mutex);
		sem_destroy(&sharedMemory->mutread);
		// detach from shared memory
		shmdt(sharedMemory);
		// clean shared memory (IPC_RMID), will be effectivelly done when all processes are detached
		shmctl(shmget(SHM_KEY, sizeof(SharedMemory), 0666), IPC_RMID, NULL);
	}
	exit(0);
}

// send data to controller, return "lost time" for update (time waited for locking semaphore and controller)
int sendDataToController(int id, CarTimeAndStatus status) {
	// variables to keep track of the time "lost" for sending the data
	struct timespec start, end;
	long alreadyWait = 0;

	// Get start time
	clock_gettime(CLOCK_MONOTONIC, &start);

	// loop until data are copied to shared memory
	while (1) {
		// Get writer exclusive access
		sem_wait(&sharedMemory->mutex);

		// check if data already treated by controller
		if (sharedMemory->carTimeAndStatuses[id].processed == false) {
			// not treated, release exclusive access and wait a few milliseconds before retry
			sem_post(&sharedMemory->mutex);
			millisWait(rand() % 20 + 1);
			// retry
			continue;
		}

		// exclusive writer access granted and data processed by controller -> copy new section time and pitStop
		memcpy(&sharedMemory->carTimeAndStatuses[id], &status, sizeof(CarTimeAndStatus));
		sharedMemory->carTimeAndStatuses[id].processed = false;

		// release exclusive access
		sem_post(&sharedMemory->mutex);

		// quit update loop
		break;
	}

	// Get end time
	clock_gettime(CLOCK_MONOTONIC, &end);

	// Compute diff in milliseconds
	alreadyWait = (end.tv_sec - start.tv_sec) * 1000; // seconds to ms
	alreadyWait += (end.tv_nsec - start.tv_nsec) / 1000000; // nanosecondes to ms

	return alreadyWait;
}

// read processed flag
bool isCarTimeAndStatusProcessed(int id) {
	bool abProcessed[MAX_PILOT];

	readSharedMemoryData(abProcessed, PROCESSED_FLAGS);

	return abProcessed[id];
}

// read sharedMemoryData
void readSharedMemoryData(void* data, enum SharedMemoryDataType smdt) {
	// get exclusive access to reader variable
	sem_wait(&sharedMemory->mutread);
	// increment reader count
	sharedMemory->readerCount++;
	if (sharedMemory->readerCount == 1) {
		// we are the first reader, we have to block the writers
		sem_wait(&sharedMemory->mutex);
	}
	// free reader access
	sem_post(&sharedMemory->mutread);

	switch(smdt) {
		case CAR_STATS:
			memcpy(data, &sharedMemory->carStats, sizeof(CarStat)*MAX_PILOT);
			break;
		case RUNNING_CARS:
			*((int*)data) = sharedMemory->runningCars;
			break;
		case CAR_TIME_AND_STATUSES:
			memcpy(data, &sharedMemory->carTimeAndStatuses, sizeof(CarTimeAndStatus)*MAX_PILOT);
			break;
		case RACE_OVER:
			*((bool*)data) = sharedMemory->raceOver;
			break;
		case PROCESSED_FLAGS:
			for (int i=0;i<MAX_PILOT;i++) {
				((bool*)data)[i] = sharedMemory->carTimeAndStatuses[i].processed;
			}
			break;
	}

	// get exclusive access to reader variable
	sem_wait(&sharedMemory->mutread);
	// decrement reader count
	sharedMemory->readerCount--;
	if (sharedMemory->readerCount == 0) {
		// No more reader, free the writers
		sem_post(&sharedMemory->mutex);
	}
	// free reader access
	sem_post(&sharedMemory->mutread);
}

int getRunningCars() {
	int runningCars;

	readSharedMemoryData(&runningCars, RUNNING_CARS);

	return runningCars;
}

void decrementRunningCars() {
	// get exclusive access to shared memory
	sem_wait(&sharedMemory->mutex);

	// decrement running cars
	sharedMemory->runningCars--;

	// relase exclusive access
	sem_post(&sharedMemory->mutex);
}

// set race as over
void setRaceAsOver() {
	// get exclusive access to shared memory
	sem_wait(&sharedMemory->mutex);

	// set race as Over
	sharedMemory->raceOver=true;

	// relase exclusive access
	sem_post(&sharedMemory->mutex);
}

bool isRaceOver() {
	bool isRaceOver;

	readSharedMemoryData(&isRaceOver, RACE_OVER);

	return isRaceOver;
}

void setCarTimeAndStatusAsProcessed(int i) {
	// get exclusive access to shared memory
	sem_wait(&sharedMemory->mutex);

	// set processed to true
	sharedMemory->carTimeAndStatuses[i].processed = true;

	// relase exclusive access
	sem_post(&sharedMemory->mutex);
}

void updateCarStat(CarStat carStat, int i) {
	// get exclusive access to shared memory
	sem_wait(&sharedMemory->mutex);

	// set processed to true
	memcpy(&(sharedMemory->carStats[i]),&carStat,sizeof(CarStat));

	// relase exclusive access
	sem_post(&sharedMemory->mutex);
}

// -------------------------------------------------------------
// used by main to display phase in english
const char* racePhaseToString(enum RacePhase phase) {
    switch (phase) {
        case START_OF_WEEKEND: return "Start of week-end";
        case FREE_PRACTICE_1: return "Free Practice 1";
        case FREE_PRACTICE_2: return "Free Practice 2";
        case FREE_PRACTICE_3: return "Free Practice 3";
        case QUALIFICATION_1: return "Qualification 1";
        case QUALIFICATION_2: return "Qualification 2";
        case QUALIFICATION_3: return "Qualification 3";
        case SPRINT_QUALIFICATION_1: return "Sprint qualification 1";
        case SPRINT_QUALIFICATION_2: return "Sprint qualification 2";
        case SPRINT_QUALIFICATION_3: return "Sprint qualification 3";
        case SPRINT:          return "Sprint";
        case RACE:            return "Race";
        default:              return "Unknown Phase";
    }
}

// special == true if week-end includes a SPRINT race
enum RacePhase getNextPhase(enum RacePhase currentPhase, bool special) {
    if (!special) {
		switch (currentPhase) {
			case START_OF_WEEKEND: return FREE_PRACTICE_1;
			case FREE_PRACTICE_1: return FREE_PRACTICE_2;
			case FREE_PRACTICE_2: return FREE_PRACTICE_3;
			case FREE_PRACTICE_3: return QUALIFICATION_1;
			case QUALIFICATION_1: return QUALIFICATION_2;
			case QUALIFICATION_2: return QUALIFICATION_3;
			case QUALIFICATION_3: return RACE;
			case RACE: return FINISH;
			default: return FINISH;
		}
	} else {
		switch (currentPhase) {
			case START_OF_WEEKEND: return FREE_PRACTICE_1;
			case FREE_PRACTICE_1: return SPRINT_QUALIFICATION_1;
			case SPRINT_QUALIFICATION_1: return SPRINT_QUALIFICATION_2;
			case SPRINT_QUALIFICATION_2: return SPRINT_QUALIFICATION_3;
			case SPRINT_QUALIFICATION_3: return SPRINT;
			case SPRINT: return QUALIFICATION_1;
			case QUALIFICATION_1: return QUALIFICATION_2;
			case QUALIFICATION_2: return QUALIFICATION_3;
			case QUALIFICATION_3: return RACE;
			case RACE: return FINISH;
			default: return FINISH;
		}
    }
}

// special == true if week-end includes a SPRINT race
enum RacePhase getPreviousPhase(enum RacePhase currentPhase, bool special) {
    if (!special) {
		switch (currentPhase) {
			case FREE_PRACTICE_1: return START_OF_WEEKEND;
			case FREE_PRACTICE_2: return FREE_PRACTICE_1;
			case FREE_PRACTICE_3: return FREE_PRACTICE_2;
			case QUALIFICATION_1: return FREE_PRACTICE_3;
			case QUALIFICATION_2: return QUALIFICATION_1;
			case QUALIFICATION_3: return QUALIFICATION_2;
			case RACE: return QUALIFICATION_3;
			case FINISH: return RACE;
		}
	} else {
		switch (currentPhase) {
			case FREE_PRACTICE_1: return START_OF_WEEKEND;
			case SPRINT_QUALIFICATION_1: return FREE_PRACTICE_1;
			case SPRINT_QUALIFICATION_2: return SPRINT_QUALIFICATION_1;
			case SPRINT_QUALIFICATION_3: return SPRINT_QUALIFICATION_2;
			case SPRINT: return SPRINT_QUALIFICATION_3;
			case QUALIFICATION_1: return SPRINT;
			case QUALIFICATION_2: return QUALIFICATION_1;
			case QUALIFICATION_3: return QUALIFICATION_2;
			case RACE: return QUALIFICATION_3;
			case FINISH: return RACE;
		}
    }
}

// display pilot ranking
void displayRanking(int raceNumber) {
	PilotStat pilotStats[MAX_PILOT];

	// load all previous data
	loadFinalChampionshipResult(raceNumber, pilotStats, drivers);
	qsort(pilotStats, MAX_PILOT, sizeof(PilotStat), comparePilotStat);

	printf("\033[11;10H┌─────────────────┐");
	printf("\033[12;10H│ Pilot's ranking │");
	printf("\033[13;10H├─────┬───────────┴──────────────────┬───────┬──────────┐");
	printf("\033[14;10H│ Pos │ Name                         │ total │ Race won │");
	printf("\033[15;10H├─────┼──────────────────────────────┼───────┼──────────┤");
	int pos=1;
	for (int i=0;i<MAX_PILOT;i++) {
		if (i>0 && pilotStats[i].score == pilotStats[i-1].score && pilotStats[i].raceWon == pilotStats[i-1].raceWon) {
			// equality, driver is at same position in ranking
		} else {
			pos=i+1;
		}
		printf("\033[%d;10H│ %3d │ %s", 16+i, pos, pilotStats[i].name);
		printf("\033[%d;47H│ %5d │ %8d │", 16+i, pilotStats[i].score, pilotStats[i].raceWon);
	}
	printf("\033[36;10H└─────┴──────────────────────────────┴───────┴──────────┘\n");
	fflush(stdout);
}

/**
 * ======================================================================================
 * = MAIN : formula 1 manager by Benjamin, Cyril, Gaylor and Simon
 * ======================================================================================
*/
int main(int argc, char *argv[]) {
	// Read track data
	readTrackData(tracks);

	// Read driver data
	readDriverData(drivers);

	// variable to store where we are in the championship
	int raceNumber;
	enum RacePhase phase;

	// display splash screen
	// displayLogo();

	// Read championship.txt to determine last race and phase done
	FILE *file = fopen("championship.txt","r");
	if (!file) {
		// file does not exist, championship not yet started
		raceNumber = 0;
		phase = START_OF_WEEKEND;
	} else {
		// Championship started, read race number and phase
		int iPhase;
		if (!fscanf(file, "race=%d\nphase=%d", &raceNumber,&iPhase)) {
			// file content is invalid, start all over
			raceNumber=0;
			phase=FREE_PRACTICE_1;
		} else {
			// raceNumber is already read
			phase=(enum RacePhase)iPhase;
		}
		fclose(file);
	}

	printf("\033[2J"); // clear screen
	printf("\033[1;1H┌──────────────────────────────────────────────────────┐");
	printf("\033[2;1H│ Formula 1 - Manager (by Benjamin/Cyril/Gaylor/Simon) │");
	printf("\033[3;1H└──┬───────────────────────────────────────────────────┴──────────────────────┐");

	// get next phase or race
	if (phase == RACE) {
		raceNumber++;
		if (raceNumber == 22) {
			printf("   !!! Championship is over !!!");
			exit(0);
		}
		phase=FREE_PRACTICE_1;
	} else {
		phase=getNextPhase(phase,tracks[raceNumber].sprint);
	}
	printf("\033[4;1H   │ Race #%d - '%s' - '%s'",raceNumber+1, tracks[raceNumber].country, tracks[raceNumber].name);
	printf("\033[5;1H   │ Phase: '%s'",racePhaseToString(phase));
	printf("\033[4;79H│");
	printf("\033[5;79H│");

	printf("\033[6;1H   └──────────────────────────────────────────────────────────────────────────┘");
	const char* REVERSE_GREEN_BOLD = "\033[7m\033[92m\033[1m";
	const char* FAINT = "\033[2m";
	if (tracks[raceNumber].sprint == true) {
		printf("\033[7;1H   ┌────┬────┬────┬────┬────────┬────┬────┬────┬──────┐");
		printf("\033[8;1H   │%s F1 \033[0m│%s Q1 \033[0m│%s Q2 \033[0m│%s Q3 \033[0m│%s SPRINT \033[0m│%s Q1 \033[0m│%s Q2 \033[0m│%s Q3 \033[0m│%s RACE \033[0m│",
			phase == FREE_PRACTICE_1 ? REVERSE_GREEN_BOLD : FAINT,
			phase == SPRINT_QUALIFICATION_1 ? REVERSE_GREEN_BOLD : FAINT,
			phase == SPRINT_QUALIFICATION_2 ? REVERSE_GREEN_BOLD : FAINT,
			phase == SPRINT_QUALIFICATION_3 ? REVERSE_GREEN_BOLD : FAINT,
			phase == SPRINT ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_1 ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_2 ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_3 ? REVERSE_GREEN_BOLD : FAINT,
			phase == RACE ? REVERSE_GREEN_BOLD : FAINT
			);
		printf("\033[9;1H   └────┴────┴────┴────┴────────┴────┴────┴────┴──────┘");
	} else {
		printf("\033[7;1H   ┌────┬────┬────┬────┬────┬────┬──────┐");
		printf("\033[8;1H   │%s F1 \033[0m│%s F2 \033[0m│%s F3 \033[0m│%s Q1 \033[0m│%s Q2 \033[0m│%s Q3 \033[0m│%s RACE \033[0m│",
			phase == FREE_PRACTICE_1 ? REVERSE_GREEN_BOLD : FAINT,
			phase == FREE_PRACTICE_2 ? REVERSE_GREEN_BOLD : FAINT,
			phase == FREE_PRACTICE_3 ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_1 ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_2 ? REVERSE_GREEN_BOLD : FAINT,
			phase == QUALIFICATION_3 ? REVERSE_GREEN_BOLD : FAINT,
			phase == RACE ? REVERSE_GREEN_BOLD : FAINT
			);
		printf("\033[9;1H   └────┴────┴────┴────┴────┴────┴──────┘");
	}

	printf("\033[10;1H Do you want to start ? (Y/N): ");

	char confirmation = getConfirmation();
    if (confirmation == 'N') {
        printf("Ok, I quit\n");
		exit(0);
	}

	int pilotRunning=getMaxPilotRunning(phase);

	// Create shared memory
	int shmid = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
	if (shmid == -1) {
		perror("Error when creating shared memory");
		exit(EXIT_FAILURE);
	}

	sharedMemory = (SharedMemory *)shmat(shmid, NULL, 0);
	if (sharedMemory == (void *)-1) {
		perror("Error when attaching to shared memory");
		exit(EXIT_FAILURE);
	}

	// if program is stopped using CTRL+C, will make sure to cleanup shared memory
	signal(SIGINT, cleanupSharedMemory);

	// Initialize shared memory
	memset(sharedMemory->carTimeAndStatuses, 0, sizeof(sharedMemory->carTimeAndStatuses));
	memset(sharedMemory->carStats, 0, sizeof(sharedMemory->carStats));

	// if in free practice or qualification 1 => all pilots are running, we don't care in which order)
	if (phase == FREE_PRACTICE_1 || phase == FREE_PRACTICE_2 || phase == FREE_PRACTICE_3
		|| phase == QUALIFICATION_1 || phase == SPRINT_QUALIFICATION_1) {
		for (int i=0;i<MAX_PILOT;i++) {
			sharedMemory->carStats[i].pilotNumber = drivers[i].id;
			sharedMemory->carStats[i].inStand = true;
		}
	} else {
		// we are in qualification 2/3 or sprint or race, order is important and based on previous result
		enum RacePhase previousPhase = getPreviousPhase(phase, tracks[raceNumber].sprint);
		loadPhaseResult(sharedMemory->carStats, raceNumber, previousPhase);
	}

	// best lap and sections time are set to 999.999
	for (int i=0;i<MAX_PILOT;i++) {
		sharedMemory->carTimeAndStatuses[i].carStatus = WAIT_IN_STAND;
		sharedMemory->carStats[i].bestLap.seconds = 999;
		sharedMemory->carStats[i].bestLap.milliseconds = 999;
		for (int j=0;j<3;j++) {
			sharedMemory->carStats[i].bestSectionTime[j].seconds = 999;
			sharedMemory->carStats[i].bestSectionTime[j].milliseconds = 999;
		}
	}

	// Mark all carTimeStatus as processed (so car simulator can add new data)
	for (int i = 0; i < MAX_PILOT; i++) {
		sharedMemory->carTimeAndStatuses[i].processed = true;
	}

	// create semaphore
	//   &sharedMemory->mutex/mutread: will be used between different processus, so located in the shared memory
	//   1: indicates that the semaphore will be used between processus and not between threads
	//   1: initial value
	sem_init(&sharedMemory->mutex, 1, 1);
	sem_init(&sharedMemory->mutread, 1, 1);
	sharedMemory->runningCars = pilotRunning;

	// After this point, we will launch multiple process, so access to shared memory will be done using specific function using semaphores
	// Launch controller
	pid_t controller_pid = fork();
	if (controller_pid == 0) {
		// execute the controller
		controller(raceNumber, phase, pilotRunning);
		exit(0);
	}

	// Launch the carSimulator
	for (int i = 0; i < pilotRunning; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			// delay is used to simulate position on the track when starting a race.  Ignored if free practise or qualification
			CarTime delay;
			delay.seconds=i/2;
			delay.milliseconds=(i*500)%1000;
			// execute the car simulator
			carSimulator(i,delay,raceNumber,phase);
			exit(0);
		}
	}

	// Launch screenManager
	pid_t screenManagerPid = fork();
	if (screenManagerPid == 0) {
		// execute screenManager
		screenManager(phase, pilotRunning);
		exit(0);
	}

	// Wait until all childs are stopped (controller/car simulators/screen manager)
	// wait return the pid of the child
	while (wait(NULL) > 0);

	// After this point, only the main function is running, all child are stopped,
	// so no need to maange concurrent access to shared memory
	// save phase result
	savePhaseResult(raceNumber, phase, pilotRunning);

	// save sprint/race ranking
	if (phase == RACE || phase == SPRINT) {
		saveChampionshipResult(raceNumber, phase);
	}

	// Save championship data
	file = fopen("championship.txt","w");
	if (!file) {
		// error while creating the file
		perror("Unable to save championship.txt");
		return 1;
	} else {
		// Save data
		fprintf(file,"race=%d\nphase=%d\n",raceNumber,phase);
	}
	fclose(file);

	// display pilot ranking
	displayRanking(raceNumber);

	// Cleanup shared memory
	cleanupSharedMemory(0);
	return 0;
}

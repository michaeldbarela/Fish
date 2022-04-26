#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
#include<string.h>
#include<unistd.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<errno.h>

#define MAX_TIME 30 // Max number of seconds for a computation to be made
#define MAX_PROCESSES 20 // Max number of processes allowed at one time
#define SHM_KEY 0x1234 // Shared memory segment key
#define SEM_KEY 0x5678 // Semaphore key
#define OBJ_PERMS ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ) // Read/Write permissions for owner or group owner
#define ROW 16 // Max number of rows for the matrix shmp
#define COL 16 // Max number of columns for the matrix shmp

// The following four global variables will be in all three source files
char (*shmp)[ROW][COL]; // Initialize a 2D char array
int shmid; // Initialize the shared memory ID
int semid; // For use with the semaphore set

// The following global vairables are meant only for this source file
bool finished; // Sets to true once computation time ends
FILE *fp; // For file IO
int processCounter; // For keeping track of how many processes are running
struct timespec tsChild; // For setting random time between pellet process creation
pid_t fish; // For use with fork function
pid_t pellet; // For use with fork function

// Prototype Functions (Comments on details are made after the main function)
static void *childPellet( void *ignored );
void SIGINT_Handler( int ignore );
void shmgetErrorDet( void );
void shmatErrorDet( void );
void shmdtErrorDet( void );
void shmctlErrorDet( void );
void initializeMatrix( void );
void printMatrix( void );
void semgetctlErrorDet( void );
void semopErrorDet( bool lock );
void semctlErrorDet( void );

int main( int argc, char *argv[] ){
    struct timespec ts; // For nanosleep function to cause small delays (make output pretty) in main function
    processCounter = 1; // Main is the first process
    printf( "swim_mill process has begun. swim_mill PID = %d.\n", getpid() ); // Prints the PID of swim_mill main

    fp = fopen( "swim_mill_results.txt", "w+" ); // Creates/Opens file to write to
    if( fp == NULL ){
        fprintf( stderr, "File failed to open.\n" );
        exit(EXIT_FAILURE);
    }

    finished = false; // Will set to true when computation time is finished
    srand( time(NULL) ); // Will use this later in the childPellet thread for random time creation
    ts.tv_sec = 0; // 0 seconds for use with nanosleep function
    ts.tv_nsec = 100000000; // 100000000 nanoseconds (100ms) for use with nanosleep function
    nanosleep( &ts, &ts ); // Little delay so the swim_mill pid displays first

    shmgetErrorDet(); // Getting shared memory set up
    shmatErrorDet(); // Attaching shared memory
    semgetctlErrorDet(); // Initializes the semaphore set
    signal( SIGINT, &SIGINT_Handler ); // Used for the CTRL C signal to end all processes
    initializeMatrix(); // Initializes a 2D array that will be shared between processes (shmp)
    nanosleep( &ts, &ts ); // Little delay so that matrix isn't initialized after fish fork

    fish = fork(); // Create and set up the fish process
    if( fish < 0 ){
        fprintf( stderr, "fish was not created.\n");
        exit(EXIT_FAILURE);
    } else if( fish > 0 ){
        // Do parent stuff
        processCounter++; // Increments the process counter up to 2 due to fork succeeding
    } else if( fish == 0 ){
        // Do child stuff
        execv("./fish", NULL);
    }
    nanosleep( &ts, &ts ); // Little delay so the fish pid will display next

    pthread_t pellet_thread; // Create a new thread that will continuously fork to create new pellets
    int code; // Error code for if the pthread_create function fails
    code = pthread_create( &pellet_thread, NULL, childPellet, NULL );
    if( code ){
        fprintf( stderr, "pthread_create failed with code %d.\n", code );
    }
    nanosleep( &ts, &ts ); // Little delay so that if error occurs then will be displayed next

    printf( "Countdown will begin now.\n" ); // swim_mill will begin counting down
    for( int seconds = 0; seconds < MAX_TIME; seconds++ ){
        // printf( "Number of active processes = %d.\n", processCounter );
        if( (MAX_TIME - seconds) > 1 ){
            printf( "There are %d seconds left before termination.\n", (MAX_TIME-seconds) );
        } else if( (MAX_TIME - seconds) == 1 ){
            printf( "There is %d second left before termination.\n", (MAX_TIME-seconds) );
            finished = true; // Computation has ended (this will be used by the childPellet thread)
        }
        printMatrix();
        sleep(1);
    }

    code = pthread_cancel( pellet_thread ); // Cancels the thread upon completion of computation time
    if( code ){
        fprintf( stderr, "pthread_cancel failed with code %d.\n", code );
    }
    code = pthread_join( pellet_thread, NULL ); // Ensures terminated thread and main thread join to avoid potential zombie processes
    if( code ){
        fprintf( stderr, "pthread_join failed with code %d.\n", code );
    }

    printf( "Final matrix appears below.\n" );
    printMatrix();
    fclose( fp ); // Closes the  file
    shmdtErrorDet(); // Detaches shared memory
    shmctlErrorDet(); // Removes shared memory
    semctlErrorDet(); // Removes semaphore set

    code = kill( fish, SIGTERM ); // Make sure fish is killed
    if( code == -1 ){
        fprintf( stderr, "Pellet processes weren't killed with kill command. %s.\n", strerror(errno) );
    }
    signal( SIGQUIT, SIG_IGN ); // Set up SIGQUIT to be ignored by Parent
    code = kill( 0, SIGQUIT ); // Kills all other child processes (pellets) and ignores swim_mill
    // If an error occurs, print a message
    if( code == -1 ){
        fprintf( stderr, "Pellet processes weren't killed with kill command. %s.\n", strerror(errno) );
    }
    wait( NULL );
    printf( "Program has ended naturally.\n" );
    exit(EXIT_SUCCESS);
}

static void *childPellet(void *ignored){
    while( 1 ){
        int numberOfProcesses = 1; // Will start this many processes
        int status; // For use with the waitpid function
        pid_t return_pid; // For use with the waitpid function
        numberOfProcesses = (rand() % 5) + 1; // Random number between 1 and 5
        numberOfProcesses = ((processCounter + numberOfProcesses) >= MAX_PROCESSES) ? 1 : numberOfProcesses;
        numberOfProcesses = (finished) ? MAX_PROCESSES : numberOfProcesses;
        // This will generate as many pellets as was randomized in the line above
        while( numberOfProcesses > 0 ){
            numberOfProcesses--;
            // Will continue to fork as long as computation hasn't ended or MAX_PROCESSES hasn't been reached
            if( !finished && (processCounter < MAX_PROCESSES) ){
                pellet = fork();
            }
            if( pellet < 0 ){
                fprintf( stderr, "pellet was not created.\n");
                exit(EXIT_FAILURE);
            } else if( pellet > 0 ){
                // Do parent stuff
                processCounter = (!finished) ? processCounter+1 : processCounter; // Increments processCounter since fork succeeded
                // Parent will wait on children to see if they exit
                return_pid = 1; // Just an intial case to make sure while loop runs
                // While loop will ensure that even if multiple pellet processes end at once, all will be tracked and decremented
                while( return_pid > 0 ){
                    return_pid = waitpid( -1, &status, WNOHANG ); // WNOHANG used to make sure parent process continues
                    // If statement will only be entered if the above waitpid function notices a pellet process exit
                    if( return_pid > 0 ){
                        processCounter--;
                        // Cheks the status message to print the appropriate messages to the .txt file
                        if( WEXITSTATUS(status) == 0 ) {
                          fprintf( fp, "pellet PID %d has been eaten by the fish.\n", return_pid );
                        } else if( WEXITSTATUS(status) == 1 ){
                          fprintf( fp, "pellet PID %d has terminated due to an error.\n", return_pid );
                        } else if( WEXITSTATUS(status) == 2 ){
                          fprintf( fp, "pellet PID %d has passed the fish.\n", return_pid );
                        } else if( WEXITSTATUS(status) == 3 ){
                          fprintf( fp, "pellet PID %d has terminated due to initializing on top of an already exisiting pellet.\n", return_pid );
                        } else {
                          fprintf( fp, "pellet PID %d has terminated due to an unknown reason. Figure it out.\n", return_pid );
                        }
                    }
                }
            } else if( !finished && pellet == 0 ){
                execv("./pellet", NULL ); // Runs the pellet process
            }
        }
        // This portion will make the thread sleep for 1 second so long as the computation time hasn't finished
        if( !finished ) {
            sleep(1);
        }
    }
}

/* CTRL C Signal Handler to kill all processes and detach/remove shared memory in the case of this interrupt
 */
void SIGINT_Handler( int ignore ){
    int code;
    shmdtErrorDet(); // Detaches shared memory
    shmctlErrorDet(); // Removes shared memory
    semctlErrorDet(); // Removes semaphore set
    code = kill( fish, SIGTERM ); // Make sure fish is killed
    if( code == -1 ){
        fprintf( stderr, "Pellet processes weren't killed with kill command. %s.\n", strerror(errno) );
    }
    signal( SIGQUIT, SIG_IGN ); // Set up SIGQUIT to be ignored by Parent
    code = kill( 0, SIGQUIT ); // Kills all other child processes (pellets) and ignores swim_mill
    // If an error occurs, print a message
    if( code == -1 ){
        fprintf( stderr, "Pellet processes weren't killed with kill command. %s.\n", strerror(errno) );
    }
    wait( NULL );
    printf( "\nYou have successfully interrupted the program.\n" );
    exit( EXIT_SUCCESS );
}

/* Gets shared memory with key
 */
void shmgetErrorDet( void ){
    shmid = shmget( SHM_KEY, sizeof(shmp), IPC_CREAT | OBJ_PERMS );
    // If a -1 is returned, then there is an issue
    if( shmid == -1 ){
        fprintf( stderr, "Error with shmget %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

/* Attaches shared memory
 */
void shmatErrorDet( void ){
    shmp = shmat( shmid, NULL, 0 );
    // If a -1 is returned, then there is an issue
    if( shmp == (void *) -1 ){
        fprintf( stderr, "Error with shmat: %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

/* Detaches shared memory
 */
void shmdtErrorDet( void ){
    // If a -1 is returned, then there is an issue
    if( shmdt( shmp ) == -1 ){
        fprintf( stderr, "Error with shmdt: %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

/* Removes the shared mmemory
 */
void shmctlErrorDet( void ){
    // If a -1 is returned, then there is an issue
    if( shmctl(shmid, IPC_RMID, 0) == -1){
        fprintf( stderr, "Error with shmctl: %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

/* Initializes the characters in a shared memory 2D array
 */
void initializeMatrix( void ){
    semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
    for( int i = 0; i < ROW; i++ ){
        for( int j = 0; j < COL; j++ ){
            (*shmp)[i][j] = 'x';
        }
    }
    semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
}

/* Prints the current characters located in the shared memory 2D array
 */
void printMatrix( void ){
    semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
    for( int i = 0; i < ROW; i++ ){
        for( int j = 0; j < COL; j++ ){
            printf( "%c", (*shmp)[i][j] );
        }
        printf("\n");
    }
    semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
}

/* Creates the semaphore set and intializes them
 */
void semgetctlErrorDet( void ){
    semid = semget( SEM_KEY, 1, IPC_CREAT | OBJ_PERMS ); // Creates semaphore ID with appropriate permissions
    // Error Detection
    if( semid == -1 ){
        fprintf( stderr, "Error with semget %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
    // Initializes semaphore as well as perform error detection
    if( semctl(semid, 0, SETVAL, 1) == -1 ){
        fprintf( stderr, "Error with semctl initialization %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

/* Perform lock/unlock operations on the opened semaphore set
 */
void semopErrorDet( bool lock ){
    struct sembuf sops; // struct with data members needed for semop system call
    // Locking blocks any other process from accessing critical region (shared 2D array)
    if( lock ){
        sops.sem_num = 0;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        if( semop(semid, &sops, 1) == -1 ){
            fprintf( stderr, "Error with semop lock operation. %s\n", strerror(errno) );
        }
    // Unlocking gives back access to critical region (shared 2D array) to other processes
    } else if( !lock ){
        sops.sem_num = 0;
        sops.sem_op = 1;
        sops.sem_flg = 0;
        if( semop(semid, &sops, 1) == -1 ){
            fprintf( stderr, "Error with semop unlock operation. %s\n", strerror(errno) );
        }
    // Should not get to this else statement (debugging)
    } else {
        printf( "How did you wind up here?\n" );
    }
}

/* Removes the semaphore set
 */
void semctlErrorDet( void ){
    // Removes semaphore set with error detection
    if( semctl(semid, 0, IPC_RMID) == -1 ){
        fprintf( stderr, "Error with semctl removiving semaphore %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
    }
}

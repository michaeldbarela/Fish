#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
#include<string.h>
#include<unistd.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<errno.h>

#define SHM_KEY 0x1234 // Shared memory segment key
#define SEM_KEY 0x5678 // Semaphore key
#define OBJ_PERMS ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ) // Read/Write permissions for owner or group owner
#define ROW 16 // Max number of rows for the matrix shmp
#define COL 16 // Max number of columns for the matrix shmp

// The following four global variables will be in all three source files
char (*shmp)[ROW][COL]; // Initialize a 2D char array
int shmid; // Initialize the shared memory ID
int semid; // For use with the semaphore set

// Prototype Functions (Comments on details are made after the main function)
void shmgetErrorDet( void );
void shmatErrorDet( void );
void semgetctlErrorDet( void );
void semopErrorDet( bool lock );

int main( int argc, char *argv[] ){
    printf( "pellet process has begun. pellet PID = %d.\n", getpid() );
    shmgetErrorDet( ); // Gets shared memory
    shmatErrorDet( ); // Attaches shared memory
    semgetctlErrorDet(); // Initializes the semaphore set

    srand( time(NULL) ); // For creating a random row and col
    int randRow, randCol; // For determining the location of the new pellet
    semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
    // Will keep randomizing the row and col till a blank 'x' is found
    do{
        randRow = rand() % ROW;
        randCol = rand() % COL;
    } while( (*shmp)[randRow][randCol] == 'P' );

    // Initial state of pellet creation
    if( (*shmp)[randRow][randCol] == 'x' ){
        // If the current location is an 'x' then the pellet will overwrite that location with 'P'
        (*shmp)[randRow][randCol] = 'P' ;
        semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
    } else if( (*shmp)[randRow][randCol] == 'P' ){
        // Else if the current location already contains a pellet, then terminate this process with code 3
        // This shouldn't occur due to the earlier do/while loop, but just in case
        semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
        exit( 3 );
    } else if( (*shmp)[randRow][randCol] == 'F' || (*shmp)[randRow][randCol] == 'E' ){
        // Else if the current location is the fish (whether 'F' or 'E') then terminate this process successfully
        (*shmp)[randRow][randCol] = 'E'; // Updates new location to 'E' (for eaten)
        semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
        printf( "Pellet has been eaten by the fish. Pellet PID = %d.\n", getpid() );
        exit( EXIT_SUCCESS );
    } else {
        // For whaever reason it gets here, at least unlock the semaphore
        semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
    }
    sleep(1);

    // Will continually update the location of the pellet
    while( 1 ){
        randRow++; // Increment the row
        semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
        if( (*shmp)[randRow][randCol] == 'x' || (*shmp)[randRow][randCol] == 'P' ){
            // If the next row is an 'x' or 'P', then update the locations
            (*shmp)[randRow - 1][randCol] = 'x'; // Updates previous location back to 'x'
            (*shmp)[randRow][randCol] = 'P'; // Updates new location to 'P'
            semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
        } else if( (*shmp)[randRow][randCol] == 'F' ){
            // Else if the next row is 'F' or 'E' then update the location as 'E' (for eaten) and exit successfully
            (*shmp)[randRow - 1][randCol] = 'x'; // Updates previous location back to 'x'
            (*shmp)[randRow][randCol] = 'E'; // Updates new location to 'E' (for eaten)
            semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
            printf( "Pellet has been eaten by the fish. pellet PID = %d.\n", getpid() );
            exit(EXIT_SUCCESS);
        } else if( randRow >= ROW ){
            // Else if the pellet reaches the last row then state it as having passed the fish
            // Ensure that the fish char isn't overwritten with an x if the passing pellet was next to it
            if( (*shmp)[randRow - 1][randCol] != 'F' && (*shmp)[randRow - 1][randCol] != 'E' ){
                (*shmp)[randRow - 1][randCol] = 'x'; // Update the previous location to 'x'
            }
            semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
            printf( "Pellet has passed the fish. pellet PID = %d.\n", getpid() );
            exit(2);
        } else {
            // For whatever reason it ends up here, at least unlock the semaphore
            semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
        }
        sleep(1);
    }
    exit(EXIT_FAILURE); // Exit with an error (If it managed to reach this)
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
    // Unlocking allows further access to critical region (shared 2D array)
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

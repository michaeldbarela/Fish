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
int findPellet( int *col );
void movement( int *col, int direction );
void semgetctlErrorDet( void );
void semopErrorDet( bool lock );

int main( int argc, char *argv[] ){
    printf( "fish process has begun. fish PID = %d.\n", getpid() );
    shmgetErrorDet( ); // Gets shared memory
    shmatErrorDet( ); // Attaches shared memory
    semgetctlErrorDet(); // Initializes the semaphore set
    struct timespec ts; // For creating a slight delay in the case of an eaten pellet
    srand( time(NULL) ); // For use in some random cases

    // Initial location of fish is bottom row in the middle column
    int row = ROW-1;
    int col = COL/2;
    semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
    (*shmp)[row][col] = 'F';
    semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
    sleep(1);

    int direction; // Will be used to determine which way the fish moves

    // Will continually update location of the fish
    while(1){
        direction = 0; // direction = 0 means stay in the same position
        semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
        (*shmp)[row][col] = 'x'; // Update current location with x
        semopErrorDet( false ); // Lock acccess to the shared memory 2D char array
        direction = findPellet( &col ); // fish determines the closest pellet
        semopErrorDet( true ); // Lock acccess to the shared memory 2D char array
        movement( &col, direction ); // fish moves in that direction determined by findPellet
        (*shmp)[ROW-1][col] = 'F'; // Updates that location with an 'F'
        semopErrorDet( false ); // Unlock acccess to the shared memory 2D char array
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

/* Finds the closest pellet based on current location of the fish
 */
int findPellet( int *col ){
    int i, j, searchLeft, searchRight, distanceLeft, distanceRight;
    // Will limit the view to the rows above the fish
    for( i = (ROW - 2); i >= 0; i-- ){
        distanceLeft = COL;
        distanceRight = COL;
        searchLeft = *col - ((ROW - 1) - i); // Determines how many number of columns the fish should look at on the left side
        searchRight = *col + ((ROW - 1) - i); // Determines how many number of columns the fish should look at on the right side
        searchLeft = (searchLeft < 0) ? 0 : searchLeft; // Limits how far left the fish can look to column 0
        searchRight = (searchRight >= COL) ? (COL-1) : searchRight; // Limits how far right the fish can look to column COL-1
        // The following if statment and two for loops will limit the view to only what can potentially be eaten (Imagine a V)
        j = *col; // Start by setting j equal to the same column as the fish
        // This if statement checks the column the fish is in for the pellet (closest pellet at that point) at row i
        if( (*shmp)[i][j] == 'P' || (*shmp)[i + 1][j] == 'E' ){
          return 0; // Stay in same position
        }
        // This particular for loop searches the left side
        for( j = (*col - 1); j >= searchLeft; j-- ){
            // If a pellet is found, will return a number that determines the direction to move
            if( (*shmp)[i][j] == 'P' ){
                distanceLeft = (*col) % j; // Distance to the first found pellet on the left side
                break;
            }
        }
        // This particular for loop searches the right side
        for( j = (*col + 1); j <= searchRight; j++ ){
            if( (*shmp)[i][j] == 'P' ){
                distanceRight = j % (*col); // Distance to the first found pellet on the right side
                break;
            }
        }
        // This set of if/else determines whether to go left or right based on the distances of the pellets
        if( distanceLeft < distanceRight ){
            return -1;
        } else if( distanceLeft > distanceRight ){
            return 1;
        } else if( distanceLeft == distanceRight && distanceLeft < COL ){
            // If they're the same distance, randomly choose left or right direction
            // srand( time(NULL) );
            int randomDir = rand() % 2; // Either 0 or 1
            randomDir = (randomDir == 0 ) ? -1 : 1; // if 0, then choose left else right
            return randomDir;
        }
    }
    // If a pellet isn't found, this section will have the fish return to the center of the row
    if( *col < (COL/2) ){
        return 1; // Go right
    } else if( *col > (COL/2) ){
        return -1; // Go left
    } else {
        return 0; // Stay in the same position
    }
    return 0; // If it makes it here (shouldn't) then stay in the same spot
}

/* Will cause the fish to move in the direction returned by the findPellet function
 */
void movement( int *col, int direction ){
    // direction = 0 means stay in the same spot
    // direction = 1 means go right
    // direction = -1 means go left
    if( direction == 1){
        if( *col < (COL-1) ){
            *col = *col + 1; // Go right
        } else {
            *col = *col; // Stay in the same spot (so the fish doesn't leave the last row)
        }
    } else if( direction == -1 ){
        if( *col > 0 ){
            *col = *col - 1; // Go left
        } else {
            *col = *col; // Stay in the same spot (so the fish doesn't leave the last row)
        }
    } else if( direction == 0 ){
        *col = *col; // Stay in the same spot
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

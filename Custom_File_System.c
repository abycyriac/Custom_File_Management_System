#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES 128
#define NAME_LENGTH 32
#define MAX_FILE_SIZE 10240000



FILE *fd;
unsigned char blocks[NUM_BLOCKS][BLOCK_SIZE];

// Defining the file information struct that will store our file directory info
struct Directory_Entry{
    int valid;
    char filename[NAME_LENGTH];
    int inode;
};

// structure to store data of files
struct Inode{
    int valid;
    int size;
    int blocks[1250];
    int hidden;
    int read_only;
    struct tm *timeStamp;
};

struct Directory_Entry * dir;
struct Inode * inodes;
int * freeBlockList; // to track free blocks
int * freeInodeList; // to track the free inodes

// initialize the directory
void initializeDirectory(){
    int i;
    for( i=0; i<NUM_FILES; i++ ){
        dir[i].valid = 0;
        dir[i].inode = -1;
        memset( dir[i].filename, 0, NAME_LENGTH);
    }
}

// initialize the Inode ist
void initializeInodeList(){
    int i;
    for( i=0; i<NUM_FILES; i++ ){
        freeInodeList[i] = 0;
    }
}

// initialize the block list
void initializeBlockList(){
    int i;
    for( i=14; i<NUM_BLOCKS; i++ ){
        freeBlockList[i] = 0;
    }
}

// initialize the inodes
void initializeInodes(){
    int i, j;
    for( i=0; i<NUM_FILES; i++ ){
        inodes[i].valid = 0;
        inodes[i].size = 0;
        inodes[i].hidden = 0;
        inodes[i].read_only = 0;
        for( j=0; j<1250; j++){
            inodes[i].blocks[j] = -1;
        }
    }
}

// finds free inodes
int findFreeInode(){
    int i;
    for( i=0; i<NUM_FILES; i++){
        if( inodes[i].valid == 0 ){
            inodes[i].valid = 1;
            return i;
        }
    }
    return -1;
}

// finds free blocks
int findFreeBlock(){
    int i;
    for( i=4; i<NUM_BLOCKS; i++){
        if( freeBlockList[i] == 0 ){
            freeBlockList[i] = 1;
            return i;
        }
    }
    return -1;
}

// finds index to a free directory
int findEmptyDirectoryIndex( ){
    int i;
    for( i=0; i<NUM_FILES; i++){
        if( dir[i].valid == 0 ){
            dir[i].valid = 1;
            return i;
        }
    }
    return -1;
}

// finds index of directory of the requested file
int findFileDirectoryIndex( char * filename ){
    int i;
    for( i=0; i<NUM_FILES; i++){
        if( strcmp( filename, dir[i].filename ) == 0 ){
            return i;
        }
    }
    return -1;
}

/*
 * Function: readline
 * Parameter: none
 * Returns: An unprocessed char string
 * Description: Gets the user input from a stdin line
 *              and returns it for processing
 */

char *readline( void )
{
	char *input = NULL;
	ssize_t buffer = 0;

	// Getline handles the buffer for us
	getline(&input, &buffer, stdin);
	return input;
}


/*
 * Function: parse_command
 * Parameter: input - A pointer to a char string that is
 *              the unprocessed user input string
 * Returns: A parsed array of char strings
 * Description: Tokenizes the user input and splits it so
 *              we can pass specific parts to relevant functions
 */

char **parse_command( char *input ){
	int i = 0;
	char **args = malloc( 255 );
	char *arg;

	arg = strtok( input, " \r\t\n" );

	// Tokenize the arguments one by one
	while ( arg != NULL )	{
		args[i++] = arg;
		arg = strtok( NULL, " \r\t\n" );
	}

	// Terminate the array in NULL so the getFile function knows how many parameters are in use
	args[i] = NULL;
	return args;
}


/*
 * Function: displayFree
 * Parameter: display - An integer that dictates whether to display
 *                      the free space or just return it
 * Returns: An integer that equals the amount of free space in the file system
 * Description: Searches through the arrayStatus array and stores the free space
 *              in a variable, then outputs the free space if requested and
 *              returns the value
 */

int displayFree( int display ){
    int freesize = 0;
    int i;
    for( i=0; i<NUM_BLOCKS; i++){
        if( freeBlockList[i] == 0 ){
            freesize += BLOCK_SIZE;
        }
    }

    if( display == 1 ){
        printf("%d bytes free.\n", freesize );
    }
    return freesize;
}

/*
 * Function: putFile
 * Parameter: filename - A char string that is the file name to load
 *                      into the filesystem
 * Returns: none
 * Description: Determines whether a file is valid to load, determines where to
 *              put it in the filesystem, and loads it
 */

void putFile( char *filename )
{
    int    status;                   // Hold the status of all return values.
    struct stat buf;                 // stat struct to hold the returns from the stat call
    status =  stat( filename, &buf );
    int start = 0;

    if( status == -1 ){
        printf("put error: File does not exist.\n");
        return;
    }
    // If stat did not return -1 then we know the input file exists and we can use it.
    if( status != -1 )
    {

        fd = fopen ( filename, "r" );
        int copy_size   = buf.st_size;

        if( displayFree(0) < copy_size ){
            printf("put error: Not enough disk space.\n");
            return;
        }
        if( copy_size > MAX_FILE_SIZE ){
            printf("put error: File size is too big.\n");
            return;
        }
        if( findFileDirectoryIndex( filename ) != -1 ){
            printf("put error: File already put in the file system.\n");
            return;
        }

        printf("Reading %d bytes from %s\n", (int) buf.st_size, filename );
        int directoryIndex = findEmptyDirectoryIndex();
        int inodeIndex = findFreeInode();
        int blockIndex = findFreeBlock();
        int offset      = 0;

        if( directoryIndex != -1 && inodeIndex != -1 && blockIndex != -1 ){
            dir[directoryIndex].inode = inodeIndex;
            strcpy( dir[directoryIndex].filename, filename);
            inodes[inodeIndex].blocks[start++] = blockIndex;
            inodes[inodeIndex].size = copy_size;
            inodes[inodeIndex].valid = 1;
            freeInodeList[inodeIndex] = 1;
            time_t rawTime;
            time ( &rawTime );
            inodes[inodeIndex].timeStamp = localtime ( &rawTime );

            while( copy_size > 0 ){
                fseek( fd, offset, SEEK_SET );
                int bytes  = fread( blocks[blockIndex], BLOCK_SIZE, 1, fd );

                if( bytes == 0 && !feof( fd ) )
                {
                    printf("put error: An error occured reading from the input file.\n");
                    return;
                }

                clearerr( fd );
                copy_size -= BLOCK_SIZE;
                offset    += BLOCK_SIZE;
                blockIndex = findFreeBlock();
                inodes[inodeIndex].blocks[start++] = blockIndex;
            }
        }
    fclose( fd );
    }
}


/*
 * Function: getFile
 * Parameters: Two char arrays, one for the file to retrieve from the file system
 *              and the other for an optional new name for the output file
 * Returns: none
 * Description: Copies a file from our file system back onto disk.  The output
 *              name is the same as the original file name by default, but is
 *              customizable using a second parameter
 */



void getFile( char *filename, char *newFilename )
{
    int    status;                   // Hold the status of all return values.
    struct stat buf;                 // stat struct to hold the returns from the stat call
    status =  stat( filename, &buf );

    int directoryIndex = findFileDirectoryIndex( filename );
    if( directoryIndex == -1 ){
        printf("get error: File not found.\n");
        return;
    }

    if( newFilename == NULL ){
        strcpy( newFilename, filename );
    }
    fd = fopen(newFilename, "w");

    if( fd == NULL )
    {
      printf("get error: Could not open output file: %s\n", newFilename );
      perror("get error: Opening output file returned.\n");
      return ;
    }

    // Initialize our offsets and pointers just we did above when reading from the file.
    int start = 0;
    int inodeIndex = dir[directoryIndex].inode;
    int blockIndex = inodes[inodeIndex].blocks[start++];


    int copy_size   = buf . st_size;
    int offset      = 0;

    printf("Writing %d bytes to %s\n", (int) buf . st_size, newFilename );

    // Using copy_size as a count to determine when we've copied enough bytes to the output file.
    // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
    // our stored data to the file fp, then we will increment the offset into the file we are writing to.
    // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
    // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
    // last iteration we'd end up with gibberish at the end of our file.
    while( copy_size > 0 )
    {
      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( blocks[blockIndex], num_bytes, 1, fd );

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      blockIndex = inodes[inodeIndex].blocks[start++];

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( fd, offset, SEEK_SET );
    }

    // Close the output file, we're done.
    fclose( fd );

  return;
}

/*
 * Function: delFile
 * Parameter: A char string that is the filename of the file to delete
 * Returns: none
 * Description: Removes a file from the filesystem.
 */

void delFile( char *filename )
{
    int directoryIndex = findFileDirectoryIndex( filename );
    if( directoryIndex == -1 ){
        printf("del error: File not found.\n");
        return;
    }

    int j = 0;
    int inodeIndex = dir[directoryIndex].inode;
    int blockIndex;

    if( inodes[inodeIndex].read_only == 1 ){
        printf("del: That file is marked read-only.\n");
        return;
    }

    inodes[inodeIndex].valid = 0;
    inodes[inodeIndex].size = 0;
    inodes[inodeIndex].hidden = 0;
    inodes[inodeIndex].read_only = 0;


    for( j=0; j<1250; j++){
        blockIndex = inodes[inodeIndex].blocks[j];
        if( blockIndex == -1 ) break;
        freeBlockList[blockIndex] = 0;
        inodes[inodeIndex].blocks[j] = -1;
    }

    freeInodeList[inodeIndex] = 0;

    dir[inodeIndex].valid = 0;
    dir[inodeIndex].inode = -1;
    memset( dir[inodeIndex].filename, 0, NAME_LENGTH);
    printf("File deleted successfully.\n");
    return;
}

/*
 * Function: createfs
 * Parameter: A char string that is the name of the filesystemb image
 * Returns: none
 * Description: creates a file system image
 */
void createfs( char * filename ){
   fd = fopen( filename, "w");
   if( fd == NULL ){
        printf("createfs: File not found.\n");
   }
   else{
        memset( &blocks[0], 0, NUM_BLOCKS*BLOCK_SIZE );
        fwrite( &blocks[0], NUM_BLOCKS, BLOCK_SIZE, fd);
        fclose(fd);
   }
}

/*
 * Function: open
 * Parameter: A char string that is the filename of the file system image to open
 * Returns: none
 * Description: open an file
 */
void open(char * fiename)
{
    fd = fopen(fiename, "rw");
    if( fd == NULL ){
        printf("open: File not found.\n");
    }
    else{
        fread(blocks, BLOCK_SIZE, NUM_BLOCKS,fd);
    }
}

/*
 * Function: file close
 * Parameter: none
 * Returns: none
 * Description: writes the filesystem to the disk and closes it
 */
void fileclose()
{
    fwrite(blocks, BLOCK_SIZE, NUM_BLOCKS, fd);
    fclose(fd);
}

void list(){
    int i, flag=0;
    char buffer [20];
    for( i=0; i<NUM_FILES; i++){
        if( dir[i].valid == 1 ){
            int inode_index = dir[i].inode;
            if( inodes[inode_index].hidden == 0 ){
                strftime( buffer, 20, "%h %d %H:%M", inodes[inode_index].timeStamp );
                printf( "%10d bytes\t%s\t%s\n", inodes[inode_index].size, buffer, dir[i].filename );
                flag = 1;
            }
        }
    }
    if( flag == 0 ){
        printf("List: No files found.\n");
    }
}

/*
 * Function: attribute
 * Parameter: a char string containing the attribute command and the a char string for the filename
 * Returns: none
 * Description: changes the attributes like read only and hidden of the files.
 */
void attribute( char * input, char * filename ){

    int directoryIndex = findFileDirectoryIndex( filename );
    if( directoryIndex == -1 ){
        printf("attrib: File not found.\n");
        return;
    }

    int inodeIndex = dir[directoryIndex].inode;

    if( strcmp( input, "+h") == 0) {
        inodes[inodeIndex].hidden = 1;
    }else if(strcmp( input, "-h") == 0) {
        inodes[inodeIndex].hidden = 0;
    }else if(strcmp( input, "-r") == 0) {
        inodes[inodeIndex].read_only = 0;
    }else if(strcmp( input, "+r") == 0) {
        inodes[inodeIndex].read_only = 1;
    }else{
        printf("Input is incorrect.\n");
    }

}

/*
 * Function: checkname
 * Parameter: A char string that is the filename to be checked
 * Returns: integer value
 * Description: checks whether the filename provided by the user goes well by the requirements
 */
int checkname( char * filename ){
    int i;
    if( filename == NULL ) return 0;
    int len = strlen(filename);
    if( len > 32 ) {
        printf("Error: File name too long.\n");
        return 0;
    }
    for( i=0; i<len; i++){
        if ( isalnum(filename[i]) == 0 ){
            if ( filename[i] != '.' ){
                printf("Error: File name is not valid.\n");
                return 0;
            }
        }
    }
    return 1;
}
/*
 * Function: main
 * Parameter: none
 * Returns: An int; 0 for success, 1 if there was an error
 * Description: The main program loop
 */

int main( void )
{
	int i, j;
    dir = (struct Directory_Entry *)&blocks[0];
    freeInodeList = (int *)&blocks[7];
    freeBlockList = (int *)&blocks[10];
    inodes = (struct Inode *)&blocks[13];

    initializeDirectory();
    initializeInodeList();
    initializeBlockList();
    initializeInodes();

	int quit = 0;
	char *rawInput;
	char **parsedInput;

	for( i=0; i<14; i++){
        freeBlockList[i] = 1;
	}

	// Main program loop
	while( quit == 0 )
	{
		// Print a prompt
		printf( "mfs> ");

		// Get input from user
		rawInput = readline();

		// Parse the input into a useable array
		parsedInput = parse_command( rawInput );

		// Find out what to do with the input array
		if ( parsedInput[0] == NULL )
			printf( "");

		else if ( strcmp( parsedInput[0], "put" ) == 0 ){
            if( checkname( parsedInput[1]) == 1 ){
                putFile( parsedInput[1] );
            }else{
            }
		}

		else if ( strcmp( parsedInput[0], "get" ) == 0 ){
            if( parsedInput[2] == NULL ){
                if( checkname( parsedInput[1]) == 1 ){
                    getFile( parsedInput[1], parsedInput[1] );
                }else{
                }
            }else
                if( checkname( parsedInput[1] ) == 1 && checkname( parsedInput[2] ) == 1 ){
                    getFile( parsedInput[1], parsedInput[2] );
                }else{
                }

		}
        else if ( strcmp( parsedInput[0], "attrib" ) == 0 )
            if( checkname( parsedInput[2]) == 1 ){
                attribute( parsedInput[1], parsedInput[2] );
            }else{
            }

		else if ( strcmp( parsedInput[0], "del" ) == 0 )
			if( checkname( parsedInput[1]) == 1 ){
                delFile( parsedInput[1] );
            }else{
            }

		else if ( strcmp( parsedInput[0], "list" ) == 0 )
			list();

		else if ( strcmp( parsedInput[0], "df" ) == 0 )
			displayFree( 1 );

		else if ( strcmp( parsedInput[0], "quit" ) == 0 )
			quit = 1;

        else if ( strcmp( parsedInput[0], "createfs" ) == 0 )
            if( checkname( parsedInput[1]) == 1 ){
                createfs(parsedInput[1]);
            }else{
            }
        else if ( strcmp( parsedInput[0], "open" ) == 0 )
            if( checkname( parsedInput[1]) == 1 ){
                open(parsedInput[1]);
            }else{
            }

        else if ( strcmp( parsedInput[0], "close" ) == 0 )
			fileclose();
		else{

            char crntpth[1500],pth1[1500],pth2[1500],pth3[1500];
            int r,status;
            pid_t child_pid= fork();

            getcwd(crntpth, sizeof(crntpth)); //storing all paths for argument 1 of execlp
            strcat(crntpth,"/");
            strcpy(pth1,"/usr/local/bin/");
            strcpy(pth2,"/usr/bin/");
            strcpy(pth3,"/bin/");

            int k, flag = 0;
            for( k=0;k<11;k++){
                if(parsedInput[i] == NULL && flag == 0){
                    for(; k<10 ; k++){
                        parsedInput[k+1] = NULL;
                        flag = 1;
                    }
                }
            }


            if(child_pid==0)
            {
                strcat(crntpth,parsedInput[0]);//adding command at end of path
                strcat(pth1,parsedInput[0]);
                strcat(pth2,parsedInput[0]);
                strcat(pth3,parsedInput[0]);
                r=execlp(crntpth,parsedInput[0],parsedInput[1],parsedInput[2],parsedInput[3],parsedInput[4],
                    parsedInput[5],parsedInput[6],parsedInput[7],parsedInput[8],parsedInput[9],parsedInput[10],NULL);
                r=execlp(pth1,parsedInput[0],parsedInput[1],parsedInput[2],parsedInput[3],parsedInput[4],
                    parsedInput[5],parsedInput[6],parsedInput[7],parsedInput[8],parsedInput[9],parsedInput[10],NULL );
                r=execlp(pth2,parsedInput[0],parsedInput[1],parsedInput[2],parsedInput[3],parsedInput[4],
                    parsedInput[5],parsedInput[6],parsedInput[7],parsedInput[8],parsedInput[9],parsedInput[10],NULL );
                r=execlp(pth3,parsedInput[0],parsedInput[1],parsedInput[2],parsedInput[3],parsedInput[4],
                parsedInput[5],parsedInput[6],parsedInput[7],parsedInput[8],parsedInput[9],parsedInput[10],NULL );

                printf("%s: command not found\n",parsedInput[0]);
            }
            waitpid( child_pid, &status, 0); //waiting for child process to end
        }

	}

	return 0 ;
}

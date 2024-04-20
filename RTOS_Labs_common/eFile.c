// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include <stdio.h>

#define MAX_FILES 32
#define BLOCK_SIZE 512
#define FAT_SIZE 2048
#define FILE_START ((FAT_SIZE*2)/BLOCK_SIZE) + 1
#define END_OF_FILE 0xFFFF
#define FREE_BLOCK 0xFFFE
#define MAX_FILE_NAME 7
#define DIR_FREE 0


extern Sema4Type LCDFree;
#define DiskMutex LCDFree

typedef enum File_State
{
	EMPTY = 0,
	USED,	
	WRITE,
	READ
} File_State;


typedef struct {
    char name[MAX_FILE_NAME + 1];
    uint16_t startBlock;
		File_State state;   // 1 if in use, 0 if not
    uint32_t size;
} DirectoryEntry;

typedef uint16_t FATEntry;

DirectoryEntry *WritePointer;
DirectoryEntry *ReadPointer;

DirectoryEntry directory[MAX_FILES];
FATEntry fatTable[FAT_SIZE];
bool fsInitialized = false;


// Helper Functions

//Writes the directory in RAM to the disk
int WriteDirectory(void){
	uint8_t buffer[BLOCK_SIZE];
	OS_bWait(&DiskMutex);
	memcpy(buffer, directory, sizeof(directory));
	if(eDisk_WriteBlock(buffer, 0)) return 1;
	OS_bSignal(&DiskMutex);
	return 0;

}

//Reads the directory on the disk into RAM
int ReadDirectory(void){
	uint8_t buffer[BLOCK_SIZE];
	//Retrieve Directory
	OS_bWait(&DiskMutex);
	if (eDisk_ReadBlock(buffer,0)) return 1;
	memcpy(directory, buffer, sizeof(buffer));
	OS_bSignal(&DiskMutex);
	return 0;
}

//Writes the FAT from RAM to disk
int WriteFAT(void){
	uint8_t buffer[BLOCK_SIZE];
	OS_bWait(&DiskMutex);
	//Write FAT Table
	for(int i = 1; i < FILE_START; i++){
		memcpy(buffer, fatTable + (i * BLOCK_SIZE / 2), sizeof(buffer));
		if(eDisk_WriteBlock(buffer, i)) return 1;
	}
	OS_bSignal(&DiskMutex);
	return 0;
}

//Reads the FAT from disk to RAM
int ReadFAT(void){
	uint8_t buffer[BLOCK_SIZE];
	//Retrieve FAT
	OS_bWait(&DiskMutex);
	for(int i = 1; i < FILE_START; i++){
		if(eDisk_ReadBlock(buffer, i)) return 1;
		memcpy(fatTable + (i * BLOCK_SIZE / 2), buffer, sizeof(buffer));
	}
	OS_bSignal(&DiskMutex);
	return 0;
}




//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	if (fsInitialized) return 1;
	
  OS_bWait(&DiskMutex);	
	if (eDisk_Init(0)) return 1;
	fsInitialized = true;
	OS_bSignal(&DiskMutex);
	return 0;

}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format

	if (!fsInitialized) eFile_Init();
	//OS_bWait(&DiskMutex);
	memset(directory, 0, sizeof(directory));
	//Clear directory
	for (int i = 0; i < MAX_FILES; i++) {
			directory[i].state = EMPTY;
	}
	//Set final directory entry
	directory[DIR_FREE].startBlock = FILE_START;
	directory[DIR_FREE].size = (FAT_SIZE * BLOCK_SIZE - (BLOCK_SIZE - FILE_START));
	directory[DIR_FREE].state = USED;
	strcpy(directory[DIR_FREE].name, "empty");
	//TODO Assign name here

	ReadPointer = NULL;
	WritePointer = NULL;

	//Fill the fat table to null state
	for (int i = 0; i < FAT_SIZE; i++) {

		//Write null if in directory or FAT Table
		if(i < FILE_START) fatTable[i] = NULL;
		else if (i == FAT_SIZE - 1) fatTable[i] = 0;
		else{
			fatTable[i] = i+1;
		}

	}
	//OS_bSignal(&DiskMutex);

	if(WriteDirectory()) return 1;

	if(WriteFAT()) return 1;

  return 0;
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system

	if (!fsInitialized) eFile_Init();

	//OS_bWait(&DiskMutex);
	memset(directory, 0, sizeof(directory));
	//Clear directory
	for (int i = 0; i < MAX_FILES; i++) {
			directory[i].state = EMPTY;
	}
	//Set final directory entry
	directory[DIR_FREE].startBlock = FILE_START;
	directory[DIR_FREE].size = (FAT_SIZE * BLOCK_SIZE - (BLOCK_SIZE - FILE_START));
	directory[DIR_FREE].state = USED;
	strcpy(directory[DIR_FREE].name, "empty");
	//TODO Assign name here

	ReadPointer = NULL;
	WritePointer = NULL;

	//Fill the fat table to null state
	for (int i = 0; i < FAT_SIZE; i++) {

		//Write null if in directory or FAT Table
		if(i < FILE_START) fatTable[i] = NULL;
		else if (i == FAT_SIZE - 1) fatTable[i] = 0;
		else{
			fatTable[i] = i+1;
		}

	}
	//OS_bSignal(&DiskMutex);
	if(ReadDirectory()) return 1;

	if(ReadFAT()) return 1;

  return 0;
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
	//Check if file already exists
	for (int i = 0; i < MAX_FILES; i++) {
		if (directory[i].state == USED && strcmp(directory[i].name, name) == 0) {
			return 2;
		}
	}
		
	for (int i = 1; i < MAX_FILES; i++) {
			if (directory[i].state == EMPTY) {
					//Fill information for new file
					strncpy(directory[i].name, name, MAX_FILE_NAME - 1);
					directory[i].name[MAX_FILE_NAME - 1] = '\0'; // Ensures string is null-terminated
					directory[i].startBlock = directory[DIR_FREE].startBlock;
					directory[i].size = 0;
					directory[i].state = USED;


					//Update Free pointer
					directory[DIR_FREE].size = directory[DIR_FREE].size - BLOCK_SIZE;
					directory[DIR_FREE].startBlock = fatTable[directory[DIR_FREE].startBlock];

					if(WriteDirectory())return 1;

					return 0;
			}
	}
  return 1;   // no free directory entries
}


uint32_t currentWriteFileLastBlock = 0;  // Last block of the currently open file for writing
uint32_t currentWriteFilePosition = 0;   // Current position within the last block (to know where to append)
uint8_t writeBuffer[BLOCK_SIZE];
//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(const char name[]) {
    if (!fsInitialized) return 1; // Check if the file system is initialized
		if (WritePointer != NULL) return 1;
    // Reset global tracking for the current file
    currentWriteFileLastBlock = 0;
    currentWriteFilePosition = 0;

    // Search for the file in the directory
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].state == USED && strcmp(directory[i].name, name) == 0) {
            // Found the file, calculate its last block based on file size
					  WritePointer = &directory[i];
						WritePointer->state = WRITE;
            uint32_t fileSize = directory[i].size;

            // Calculate the number of blocks occupied by the file
            uint32_t blocksNeeded = fileSize / BLOCK_SIZE;
						currentWriteFilePosition = fileSize % BLOCK_SIZE;

            // Traverse the FAT to find the last block of the file
            uint16_t block = directory[i].startBlock;
            uint32_t nextBlock = block;
            for (uint32_t j = 0; j < blocksNeeded; j++) { 
                block = nextBlock;
                nextBlock = fatTable[block]; // Get the next block in the FAT chain
            }
            currentWriteFileLastBlock = nextBlock; // The last block in which data was written
						
						//Write to the write buffer
						memset(writeBuffer,0,sizeof(writeBuffer));
						OS_bWait(&DiskMutex);
						if(eDisk_ReadBlock(writeBuffer, currentWriteFileLastBlock)) return 1;
						OS_bSignal(&DiskMutex);

            return 0; // Success
        }
    }

    return 1; // File not found or another failure
}



//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
		//Check if write is open
		if(WritePointer == NULL){
			return 1;
		}
		writeBuffer[currentWriteFilePosition] = data;
		currentWriteFilePosition += 1;
		WritePointer->size += 1;
		if(currentWriteFilePosition == BLOCK_SIZE){//Block is full, allocate new one
				//Write the block to memory
				OS_bWait(&DiskMutex);
				if(eDisk_WriteBlock(writeBuffer, currentWriteFileLastBlock))return 1;
				OS_bSignal(&DiskMutex);
			  memset(writeBuffer,0,sizeof(writeBuffer));
				currentWriteFilePosition = 0;
			
			
			  fatTable[currentWriteFileLastBlock] = directory[DIR_FREE].startBlock;
				currentWriteFileLastBlock = fatTable[currentWriteFileLastBlock];
				//Update Free pointer
				directory[DIR_FREE].size = directory[DIR_FREE].size - BLOCK_SIZE;
				directory[DIR_FREE].startBlock = fatTable[directory[DIR_FREE].startBlock];

				if(WriteDirectory())return 1;
				if(WriteFAT())return 1;
			
		}
    return 0;   // replace
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	//Write the current buffer
	if(WritePointer == NULL) return 1;
	OS_bWait(&DiskMutex);
	if(eDisk_WriteBlock(writeBuffer, currentWriteFileLastBlock))return 1;
	OS_bSignal(&DiskMutex);
	WritePointer->state = USED;
	WritePointer = NULL;
	if(WriteDirectory())return 1;
	if(WriteFAT())return 1;

  return 0;   // replace
}



uint32_t currentReadFileLastBlock = 0;  // Last block of the currently open file for writing
uint32_t currentReadFilePosition = 0;   // Current position within the last block (to know where to append)
uint32_t readRemaining = 0;
uint8_t readBuffer[BLOCK_SIZE];
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
    if (!fsInitialized) return 1; // Check if the file system is initialized
		if (ReadPointer != NULL) return 1;
    // Reset global tracking for the current file
    currentReadFileLastBlock = 0;
    currentReadFilePosition = 0;

    // Search for the file in the directory
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].state == USED && strcmp(directory[i].name, name) == 0) {
            // Found the file, calculate its last block based on file size
					  ReadPointer = &directory[i];
						ReadPointer->state = READ;
            uint32_t fileSize = directory[i].size;
						currentReadFileLastBlock = ReadPointer->startBlock;
						readRemaining = ReadPointer->size;
						//Write to the write buffer
						memset(readBuffer,1,sizeof(readBuffer));
						OS_bWait(&DiskMutex);
						if(eDisk_ReadBlock(readBuffer, currentReadFileLastBlock)) return 1;
						OS_bSignal(&DiskMutex);

            return 0; // Success
        }
    }

    return 1; // File not found or another failure

}

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
		//Check if write is open
		if(ReadPointer == NULL || readRemaining == 0){
			return 1;
		}
		if(currentReadFilePosition == BLOCK_SIZE){ //Need to read the next block into memory?
				currentReadFileLastBlock = fatTable[currentReadFileLastBlock];
			
			
				memset(readBuffer,0,sizeof(readBuffer));
				OS_bWait(&DiskMutex);
				if(eDisk_ReadBlock(readBuffer, currentReadFileLastBlock)) return 1;
				OS_bSignal(&DiskMutex);
				currentReadFilePosition = 0;
		}
		*pt = readBuffer[currentReadFilePosition];
		currentReadFilePosition += 1;
		readRemaining -= 1;

    return 0;   // replace	
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	if(ReadPointer == NULL) return 1;
	
	ReadPointer->state = USED;
	ReadPointer = NULL;

  return 0;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	// Search for the file in the directory
	for (int i = 0; i < MAX_FILES; i++) {
			if (directory[i].state != EMPTY && strcmp(directory[i].name, name) == 0) {
				
					//Free the space
					directory[DIR_FREE].size += directory[i].size;
					// Found the file, calculate its last block based on file size
					uint32_t fileSize = directory[i].size;

					// Calculate the number of blocks occupied by the file
					uint32_t blocksNeeded = fileSize / BLOCK_SIZE;

					// Traverse the FAT to find the last block of the file
					uint16_t block = directory[i].startBlock;
					uint32_t nextBlock = block;
					for (uint32_t j = 0; j < blocksNeeded; j++) { 
							block = nextBlock;
							nextBlock = fatTable[block]; // Get the next block in the FAT chain
					}
					fatTable[nextBlock] = directory[DIR_FREE].startBlock; // The last block in which data was written
					directory[DIR_FREE].startBlock = directory[i].startBlock;
					directory[i].state = EMPTY;
					if(WriteDirectory())return 1;
					if(WriteFAT())return 1;					
				


					return 0; // Success
			}
	}
  return 1;   // replace
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)


uint8_t current_dir_entry = 1;
// 
int eFile_DOpen( const char name[]){ // open directory
	current_dir_entry = 1;
  return 0;   // replace
}

//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)


int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
	
	for(int i = current_dir_entry; i < MAX_FILES; i++){
		if(directory[i].state != EMPTY){
			*name = directory[i].name;
			*size = directory[i].size;
			current_dir_entry = i + 1;
			return 0;
		}
	}
  return 1;  
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)

int eFile_DClose(void){ // close the directory
	current_dir_entry = 1;
  return 0;   // replace
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){
	if(!fsInitialized)return 1;
	
	if(WriteDirectory())return 1;
	if(WriteFAT())return 1;
	fsInitialized = 0;
  return 0;   // replace
}
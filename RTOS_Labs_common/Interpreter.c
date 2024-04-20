// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Lab5_ProcessLoader/loader.h"

#define MAX_ARGS 5


//Define the possible command types
typedef enum {
    CMD_HELP,
    CMD_TIME,
		CMD_ADC,
		CMD_MSG,
		CMD_CLEAR,
    CMD_UNKNOWN,
		CMD_FORMAT,
		CMD_MOUNT,
		CMD_TOUCH,
		CMD_WRITE,
		CMD_CAT,
		CMD_RM,
		CMD_LS,
		CMD_LP
} CommandType;


//--------------getCommandType---------------
// When passed a command string, returns the enum
// command type to be parsed with a switch statement
// input: command string
// output: corresponding CommandType
//--------------------------------------------
CommandType getCommandType(const char* command) {
    if (strcmp(command, "help") == 0) return CMD_HELP;
    if (strcmp(command, "time") == 0) return CMD_TIME;
		if (strcmp(command, "adc") == 0) return CMD_ADC;
		if (strcmp(command, "msg") == 0) return CMD_MSG;
		if (strcmp(command, "clear") == 0) return CMD_CLEAR;
		if (strcmp(command, "format") == 0) return CMD_FORMAT;
		if (strcmp(command, "mount") == 0) return CMD_MOUNT;
		if (strcmp(command, "touch") == 0) return CMD_TOUCH;
		if (strcmp(command, "write") == 0) return CMD_WRITE;
		if (strcmp(command, "cat") == 0) return CMD_CAT;
		if (strcmp(command, "rm") == 0) return CMD_RM;
		if (strcmp(command, "ls") == 0) return CMD_LS;
		if (strcmp(command, "lp") == 0) return CMD_LP;	
    return CMD_UNKNOWN;
}




void CmdLine(void);

void parseCommand(char* input, char** command, char** args, uint32_t* argCount);


// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)

}

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

//---------------------OutHelp---------------------
// Outputs to UART the help commands
// Input: none
// Output: none
void OutHelp(void){
	OutCRLF();
  UART_OutString("Command list:"); OutCRLF();
	UART_OutString("t - displays OS MsTime"); OutCRLF();
	UART_OutString("a - displays latest recorded ADC value"); OutCRLF();
	UART_OutString("m - Message function. Follow on screen prompts to display a message"); OutCRLF();
  UART_OutString("h - help, relists commands"); OutCRLF();
}
//---------------------MessageInput---------------------
// Communicates with the user to determine the proper
// message to display
// Input: none
// Output: none
void MessageInput(void){
	int flag = 1;
	char string[20];
	uint32_t display, line, num;
	while (flag){
		UART_OutString("Which display? (0 or 1): ");
		display = UART_InUDec(); OutCRLF();
		if (display > 1){
			UART_OutString("Invalid Selection"); OutCRLF();
		}
		else{
			flag = 0;
		}
	}
	flag = 1;

	while (flag) {
		UART_OutString("Which line? (0 to 7): ");
		line = UART_InUDec(); OutCRLF();
		if (line > 7){
			UART_OutString("Invalid Selection"); OutCRLF();
		}
		else{
			flag = 0;
		}
	}
	flag = 1;
	UART_OutString("Type a string to display: ");
	UART_InString(string, 19); OutCRLF();
	UART_OutString("Type a number to display: ");
	num = UART_InUDec(); OutCRLF();
	ST7735_Message(display, line, string, num);
}





//-----------parseCommand-----------------
// Takes in an input command string, and parses it into its main commands and arguments
// Arguments are delimited by spaces, unless encircled by quotes
// Input: the typed command string
//				A pointer to a string to hold the command
//				A pointer to an array that will hold args
//        An int pointer holding the number of args parsed
// Output: none
//-------------------------------------------
void parseCommand(char* input, char** command, char** args, uint32_t* argCount) {
	*command = strtok(input, " ");
	*argCount = 0;

	char *ptr = strtok(NULL, "");
	while (*argCount < MAX_ARGS && ptr && *ptr) {
		while (*ptr == ' ') ptr++;  // Skip spaces

		if (*ptr == '\"') {  // Start of a quoted argument
				ptr++;  // Skip the opening quote
				args[*argCount] = ptr;  // Start of the argument
				while (*ptr && *ptr != '\"') ptr++;  // Move to the end of the argument
				if (*ptr) *ptr++ = '\0';  // Null-terminate and move to the next character
		} else {
				args[*argCount] = ptr;  // Start of an unquoted argument
				while (*ptr && *ptr != ' ') ptr++;  // Move to the end of the argument
				if (*ptr) *ptr++ = '\0';  // Null-terminate and move to the next character
		}

		(*argCount)++;  // Increase argument count
	}
}

//------------------help---------------------
// This function handles the help command
// Args: zero or one. If zero, prints all command options
// If one argument, and the argument is the name of another command,
// it prints detailed info on the named command
// -----------------------------------------
void help(char** args, uint32_t* argCount){
	//Is help requested for a specific function?
	if(*argCount > 0){
		switch(getCommandType(args[0])){
			case(CMD_HELP):
				break;
			case(CMD_ADC):
				UART_OutString("adc: Prints most recently recorded ADC value"); OutCRLF();
				return;
			case(CMD_MSG):
				UART_OutString("Command: Message (msg)"); OutCRLF();
				UART_OutString("arg0: \"top\" or \"bottom\" to choose which display to use"); OutCRLF();
				UART_OutString("arg1: A number 0-7 to choose which line to write to"); OutCRLF();
				UART_OutString("arg2: A number (int32_t) to print a the end of the string. Type \"none\" for no number"); OutCRLF();
				UART_OutString("arg3: A string to print, surrounded in quotes. eg. \"String to print\""); OutCRLF();
				UART_OutString("example input: \"msg top 3 42 \"The answer to life, the universe, and everything is \"\""); OutCRLF();
				UART_OutString("Clear the display: \"msg clear\""); OutCRLF();
				return;
			case(CMD_TIME):
				UART_OutString("Format: \"time arg0\". If arg0 is \"reset\" then the OS_MsTime is reset"); OutCRLF();
				UART_OutString("Otherwise, the OS time in Ms since boot or last reset is displayed"); OutCRLF();
				return;
			case(CMD_CLEAR):
				UART_OutString("Clears terminal display"); OutCRLF();
				return;
			default:
				break;
		}
	}
	UART_OutString("Command list:"); OutCRLF();
	UART_OutString("time   - displays OS MsTime"); OutCRLF();
	UART_OutString("adc    - displays latest recorded ADC value"); OutCRLF();
	UART_OutString("msg    - Message function. Type \"help msg\" for instructions"); OutCRLF();
  //UART_OutString("help   - relists commands. If given an argument of another function name, "); OutCRLF();
	//UART_OutString("         additional details of the named function will be given"); OutCRLF();
	UART_OutString("clear  - Clears terminal display"); OutCRLF();
	UART_OutString("format - Formats the disk"); OutCRLF();
	UART_OutString("mount  - Mounts the disk. Assumes proper formatting"); OutCRLF();
	UART_OutString("write  - {filename} {text} Writes the inputted string to file"); OutCRLF();
	UART_OutString("ls     - Lists files on the disk"); OutCRLF();
	UART_OutString("touch  - {filename} Creates a file with the given name"); OutCRLF();
	UART_OutString("cat    - {filename} Prints contents of the named file"); OutCRLF();
	UART_OutString("rm     - {filename} Deletes the named file"); OutCRLF();
	UART_OutString("lp     - {filename} {# of times to load} Loads a program from the named .axf file"); OutCRLF();
	return;
}

//------------------time---------------------
// This function handles the time command
// Args: zero or one. If zero, prints the current time
// If one argument, and the argument is reset, the OS time
// is reset
// -----------------------------------------
void time(char** args, uint32_t* argCount){
	if(*argCount > 0){
		if(strcmp(args[0], "reset") == 0){
			OS_ClearMsTime();
			UART_OutString("MsTime Reset");OutCRLF();
			return;
		}
	}
	UART_OutString("Current MsTime: "); UART_OutUDec(OS_MsTime()); UART_OutString(" ms"); OutCRLF();
}

//------------------adc---------------------
// This function handles the adc command
// This command prints the latest recorded
// ADC value to the UART
// Args: none
// -----------------------------------------
void adc(){
	//UART_OutString("Current ADC Value: "); UART_OutUDec(ADC_InHW()); UART_OutString("/4096"); OutCRLF();
}

//------------------clear---------------------
// This function clears the terminal display
// Args: none
// -----------------------------------------
void clear(){
	UART_OutString("\x1B[2J\x1B[H");
}

void format(int argc, char *argv[]){
	if(eFile_Format()){
		UART_OutString("Formatting Error"); OutCRLF();
	}
	else{
		UART_OutString("Drive Successfully Formatted"); OutCRLF();
	}
}

void mount(int argc, char *argv[]){
	if(eFile_Mount()){
		UART_OutString("Mounting Error"); OutCRLF();
	}
	else{
		UART_OutString("Drive Successfully Mounted"); OutCRLF();
	}
}
void ls(int argc, char *argv[]){
	char string1[]="Filename = ";
	char string2[]="File size = ";
	char string3[]="Number of Files = ";
	char string4[]="Number of Bytes = ";
	char *name; unsigned long size; 
  unsigned int num;
  unsigned long total;
  num = 0;
  total = 0;
  UART_OutString("\n\r");
  eFile_DOpen("");
  while(!eFile_DirNext(&name, &size)){
		UART_OutString(string1); UART_OutString(name);
    UART_OutString("  ");
		UART_OutString(string2); UART_OutSDec(size);
    UART_OutString("\n\r");
    total = total+size;
    num++;    
  }
	UART_OutString(string3); UART_OutSDec(num);
  UART_OutString("\n\r");
	UART_OutString(string4); UART_OutSDec(total);
  UART_OutString("\n\r");
  eFile_DClose();
}

void touch(int argc, char *argv[]){
	if (argc == 1){

		int value = eFile_Create(argv[0]);
		if(value == 2){
			UART_OutString("File already exists"); OutCRLF();

		}
		else if(value == 1){
			UART_OutString("No remaining space for files"); OutCRLF();

		}
		else {
			UART_OutString("File successfully created"); OutCRLF();
		}
		
	}
	else{
		UART_OutString("Usage: touch {filename}"); OutCRLF();
	}
}

void write(int argc, char *argv[]) {
    if (argc != 2) {
        UART_OutString("Usage: write {filename} {text}"); OutCRLF();
        return;
    }


    int result = eFile_WOpen(argv[0]);
    if (result != 0) {
        UART_OutString("Failed to open file for writing"); OutCRLF();
        return;
    }

    for (int i = 0; argv[1][i] != '\0'; i++) {
        result = eFile_Write(argv[1][i]);
        if (result != 0) {
            UART_OutString("Failed to write to file"); OutCRLF();
            eFile_WClose(); // Attempt to close the file even if writing failed
            return;
        }
    }

    result = eFile_WClose();
    if (result != 0) {
        UART_OutString("Failed to close the file"); OutCRLF();
    } else {
        UART_OutString("Text successfully written to file"); OutCRLF();
    }
}


void cat(int argc, char *argv[]) {
    if (argc != 1) {
        UART_OutString("Usage: cat {filename}"); OutCRLF();
        return;
    }

    int result = eFile_ROpen(argv[0]);
    if (result != 0) {
        UART_OutString("Failed to open file for reading"); OutCRLF();
        return;
    }

    char data; // Variable to store data read from the file
    while (true) {
        result = eFile_ReadNext(&data);
        if (result == 0) {
            UART_OutChar(data); // Print the character to UART
        } else {
            break; // Break the loop if end of file or an error occurred
        }
    }

    result = eFile_RClose();
    if (result != 0) {
        OutCRLF();UART_OutString("Failed to close the file"); OutCRLF();
    } else {
        OutCRLF();UART_OutString("\nFile read successfully"); OutCRLF();
    }
}

void rm(int argc, char *argv[]) {
    if (argc != 1) {
        UART_OutString("Usage: rm {filename}"); OutCRLF();
        return;
    }
    int result = eFile_Delete(argv[0]);
    if (result == 0) {
        UART_OutString("File successfully deleted"); OutCRLF();
    } else {
        UART_OutString("Failed to delete file"); OutCRLF();
    }
}

void lp(int argc, char *argv[]) {
    if (argc != 2) {
        UART_OutString("Usage: lp {filename}{# of times to load}"); OutCRLF();
        return;
    }
		char* end;
		long num = strtol(argv[1], &end, 10);
		
		if(end == argv[1]){
			  UART_OutString("Usage: lp {filename}{# of times to load}"); OutCRLF();
        return;
		}
		
		static const ELFSymbol_t symtab[] = {
			{ "ST7735_Message", ST7735_Message } // address of ST7735_Message
		};

		
		ELFEnv_t env = { symtab, 1 }; // symbol table with one entry
		for(int i = 0; i < num; i++){
			if (exec_elf(argv[0], &env) == -1){
				UART_OutString("Unable to load from file"); OutCRLF();
			return;
		}
		}

		UART_OutString("Process Successfully Loaded"); OutCRLF();
		
		

}



//------------------msg---------------------
// This function handles the msg command
// arg0: \"top\" or \"bottom\" to choose which display to use
// arg1: A number 0-7 to choose which line to write to"); OutCRLF();
// arg2: A number (int32_t) to print at the end of the string. Type "none" for no number
// arg3: A string to print, surrounded in quotes. eg. "String to print"
// example input: msg top 3 42 "The answer to life, the universe, and everything is "
// Clear the display: "msg clear"
// -----------------------------------------
void msg(char** args, uint32_t* argCount){ uint32_t display, line; int32_t value;
	//Enough args?
	if(*argCount < 4){
		if(*argCount > 0 && strcmp(args[0],"clear") == 0){
			UART_OutString("Display Cleared");OutCRLF();
			ST7735_FillScreen(0);
			return;
		}
		args[0] = "msg"; *argCount = 1; help(args, argCount);
		return;
	}
	// Top or bottom selected?
	if(strcmp(args[0], "top") == 0){
		display = 0;
	}
	else if(strcmp(args[0], "bottom") == 0){
		display = 1;
	}
	else {
		args[0] = "msg"; *argCount = 1; help(args, argCount);
		return;
	}
	//Which line?
	char* end;
	long val = strtol(args[1],&end,10);
	line = (uint32_t)val;
	//Incorrect line input
	if(end == args[1] || line > 7){
		args[0] = "msg"; *argCount = 1; help(args, argCount);
		return;		
	}

	val = strtol(args[2],&end,10);
	value = (int32_t)val;
	//Not a number
	if(end == args[2]){
		if(strcmp(args[2], "none") == 0){//No number to print
			value = NULL;
		}
		else{		//Invalid input
			args[0] = "msg"; *argCount = 1; help(args, argCount);
			return;
		}			
	}
	//Display parsed message
	ST7735_Message(display, line, args[3], value);
	UART_OutString("Message Displayed"); OutCRLF();
	return;
}


// *********** Command line interpreter (shell) ************
void Interpreter(void){
	clear();
	while(1){
		CmdLine();
	}
}


// ************** Command Line Code to be used in the Interpreter **********
void CmdLine(void){
	//Define variables to handle argument inputs
	char string[100];
	char* command;
	char* args[MAX_ARGS];
	uint32_t argCount;


	UART_OutString("--> ");
	UART_InString(string,99); OutCRLF(); //Cmdline input
	parseCommand(string, &command, args, &argCount);
	//UART_OutString("Command: "); UART_OutString(command); OutCRLF();
	//UART_OutString("Num Args: "); UART_OutUDec(argCount); OutCRLF();
	//for(uint32_t i = 0; i < argCount; i++){
	//	UART_OutString("Arg"); UART_OutUDec(i); UART_OutString(": "); UART_OutString(args[i]);
	//	OutCRLF();
	//}
	switch(getCommandType(command)){
		case(CMD_HELP):
			help(args, &argCount);
			break;
		case(CMD_ADC):
			adc();
			break;
		case(CMD_MSG):
			msg(args, &argCount);
			break;
		case(CMD_TIME):
			time(args, &argCount);
			break;
		case(CMD_CLEAR):
			clear();
			break;
		case(CMD_FORMAT):
			format(argCount, args);
			break;
		case(CMD_MOUNT):
			mount(argCount, args);
			break;
		case(CMD_TOUCH):
			touch(argCount, args);
			break;
		case(CMD_WRITE):
			write(argCount, args);
			break;
		case(CMD_CAT):
			cat(argCount, args);
			break;
		case(CMD_RM):
			rm(argCount, args);
			break;
		case(CMD_LS):
			ls(argCount, args);
			break;
		case(CMD_LP):
			lp(argCount, args);
			break;
		default:
			UART_OutString("Unknown Command. Type \"help\" to get a valid command list"); OutCRLF();
			break;

	}	
}
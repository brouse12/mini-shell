#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_HISTORY 10

// Maximum allowable input size for a mini-shell command.
const int BUFFER_SIZE = 80;

// Maximum allowable number of tokens (space-separated values) in a
// mini-shell command.
const int MAX_TOKENS = 10;

// History of last ten commands entered
char* command_history[MAX_HISTORY];
int histIndex = 0;
void freeHistory(); // signature of command to free history log



//----------------------Built-in Shell Commands--------------------//
// Built-in exit function. Frees the user's input, which is a dynamically-
// allocated array of strings, and then exits with a printed message.
void myExit(char** command) {
	free(command);
	freeHistory();
	printf("Exiting mini-shell\n");
	exit(0);
}

// Built-in change directory (cd) function. cd argument must be the
// first value in an array of strings.
void myCD(char** command) {
	int outcome = chdir(command[1]);
	if (outcome < 0) {
		printf("Not a valid directory.\n");
	}
}

// Built-in function - prints out the last 10 commands entered.
void myHistory() {
	printf("Printing last 10 commands:\n");
	int count = histIndex;
	while (count < MAX_HISTORY) {
			if (strcmp(command_history[count], "None") != 0) {
				printf("%s\n", command_history[count]);
			}	
			count++;
	}
	count = 0;
	while (count < histIndex) {
		printf("%s\n", command_history[count]);
		count++;
	}
}

// Built-in help function. Lists available built-in commands.
void myHelp(){
	printf("Available built-in commands are:\n"
		"cd - change directory [directory]\n"
		"history - print last 10 terminal commands\n"
		"help - the command you just typed!\n"
		"exit - exits the mini-shell\n");

}

//------------Helper functions for shell (not called by user)----//

// Create a signal handler that should be set up to expect a 
// SIGINT signal (i.e. ^C from the user). Exits the program.
// --params: signal to be caught
void sigint_handler(int sig){
	write(1,"\nMini-shell terminated.\n", 25); 
 	freeHistory();	
	exit(0);
}

// Scanner function to parse shell input string.
// --params: and a string
// --returns: a string array formatted for use with execvp. Return value
// must be freed.
char** parse_input(char* input) {
	char** exec_argv = malloc(MAX_TOKENS * sizeof(char*));

	int count = 0;
	char delimiter[]= " \t\r\n\v\f";
	char* token = strtok(input, delimiter);
	while (token != NULL && count < MAX_TOKENS) {
		exec_argv[count] = token;
		count++;
		token = strtok(NULL, delimiter);
	}
	exec_argv[count] = NULL;
	return exec_argv;
}

// Checks if there is a pipe ("|") command in the input array
// of argument strings. Returns 1 if true, else 0. Assumes that
// input will be terminated with NULL, as required for execvp
// input.
int hasPipe(char** args) {
	int pipe = 0;
	int count = 0;
	while (args[count] != NULL) {
		if (strcmp(args[count], "|") == 0 ) {
			pipe = 1;
		}
		count++;
	}
	return pipe;
}

// Frees memory allocated for tracking command history
void freeHistory(){
	int i;
	for(i=0; i < MAX_HISTORY; i++){
		free(command_history[i]);
	}
	
}

//-----------------------------------------------------------------//


// Main function for a custom shell program.
int main(void) {

	// Allocate memory for logging past 10 commands
	char noCommand[] = "None";
	int j;
	for (j=0; j < MAX_HISTORY; j++) {
		char* array = malloc(BUFFER_SIZE * sizeof(char));
		command_history[j] = array;
		strcpy(command_history[j], noCommand);
	} 

	// Install our signal handler
	signal(SIGINT, sigint_handler);

	//List of built-in function names, with corresponding function pointers
	char* BUILTIN_NAMES[] = {"cd", "exit", "history", "help"};
	void (*FUNC_PTR[])() = {myCD, myExit, myHistory, myHelp};
	const int NUM_BUILTINS = 4;

	// Loop the shell until exit is called or ^C sends SIGINT
	while(1) {
		// Get user input and parse for shell commands
		printf("mini-shell>");
		char input[BUFFER_SIZE];
		fgets(input, BUFFER_SIZE, stdin);
		char** exec_argv = parse_input(input);
		char* command = exec_argv[0];
		
		// Track command history (last ten commands)
		strcpy(command_history[histIndex], command);
		histIndex = (histIndex + 1) % MAX_HISTORY;
	
		// If input is calling a built-in command, run it
		int isBuiltin = 0;
		int i;
		for (i=0; i < NUM_BUILTINS; i++) {
			if (strcmp(command, BUILTIN_NAMES[i]) == 0) {
				(*FUNC_PTR[i])(exec_argv);
				isBuiltin = 1;
				}
		}
		if (isBuiltin){
			free(exec_argv);
			continue;
		}
	
		// If input does not have a pipe('|'), attempt to run it as a
		// system command.
		if(!hasPipe(exec_argv)) { 
			pid_t pid;
			pid = fork();
			if(pid == 0) {
				execvp(command, exec_argv);
				printf("Command not found--Did you mean"
				" something else?\n");
				free(exec_argv);
				freeHistory();
				exit(1);
			}
			waitpid(pid, NULL, 0);
		// If there is a pipe, run both commands as system commands with a
		// pipe in-between (max 1 pipe).
		}else {
			//Set up exec_argv to be command 1, and commandTwo to be
			//command 2 (exec_argv command output is piped to
			//commandTwo).
			int count = 0;
			while(strcmp(exec_argv[count], "|") !=0) {
				count++;
			}
			exec_argv[count] = NULL;
			count++;
			char* commandTwo[MAX_TOKENS];
			int countTwo = 0;
			while(exec_argv[count] != NULL) {
				commandTwo[countTwo] = exec_argv[count];
				count++;
				countTwo++;
			}
			commandTwo[countTwo] = NULL;
			
			// Check for issues and report them as applicable
			if (exec_argv[0] == NULL || commandTwo[0] == NULL) {
				printf("There must be a command on each side"
					" of a pipe('|').\n");
				free(exec_argv);
				continue;
			}
			if (hasPipe(commandTwo)) {
				printf("Command line input can only have one"
					" pipe('|').\n");
				free(exec_argv);
				continue;
			}
			// Pipe command 1's output to command 2
			int fileDescriptors[2]; // Open the pipe
			if(pipe(fileDescriptors) == 0) {
				pid_t pid;
				pid = fork();
				if(pid == 0) {
					close(STDOUT_FILENO);
					dup(fileDescriptors[1]);
					close(fileDescriptors[0]);
					close(fileDescriptors[1]);
					execvp(exec_argv[0], exec_argv);
					fprintf(stderr, "First command not found.\n");
					free(exec_argv);
					freeHistory();
					exit(1);
				}
				pid = fork();
				if(pid == 0) {
					close(STDIN_FILENO);
					dup(fileDescriptors[0]);
					close(fileDescriptors[1]);
					close(fileDescriptors[0]);
					execvp(commandTwo[0], commandTwo);
					fprintf(stderr, "Second command not found.\n");
					free(exec_argv);
					freeHistory();
					exit(1);
				}
				// Close pipe
				close(fileDescriptors[0]);
				close(fileDescriptors[1]);
				wait(NULL);
				wait(NULL);
			}else {
				fprintf(stderr, "Error, could not create"
					" pipe. Try again.\n");
			}	
		}
		free(exec_argv);
	}
	freeHistory();
	return 0;}

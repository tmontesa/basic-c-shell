#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* =============================================================*/
/* Defined max values for arguments, commands, and buffer size! */
/* =============================================================*/

#define MAX_ARGS 128	// Max number of arguments per command.
#define MAX_CMDS 128	// Max number of commands (piped) per entry.
#define MAX_BUFF 1024	// Max number of chars for input buffer.
#define MAX_HIST 10	// Max number of history entries recorded.

/* =============================================================*/
/* Global variables!                                            */
/* =============================================================*/

char  buff[MAX_BUFF];	// Buffer for input.
char* args[MAX_ARGS];	// Where command arguments are stored.
char* cmds[MAX_CMDS];	// Where commands are stored.
char* hist[MAX_HIST];	// Where history is stored.
pid_t pids[MAX_CMDS];	// Where children's pids are stored for waiting.

int pipes[2];		// Pipes for piping.
int num_pipe = 0;	// Number of pipes in a command.
int temp_pipe = -1;	// Temp holds FD for a pipe. 
int pipe_pos = 0;	// Position on pipeline.
int hist_pos = 0;


/* =============================================================*/
/* Modulo with negative numbers. Needed for circular array!     */
/* =============================================================*/

int modn(int a, int b) {
	int x = a % b;
	if (x < 0) {
		x += b;
	}
	return x;
}

/* =============================================================*/
/* Captures signals and prevents them from doing a thing.       */
/* -------------------------------------------------------------*/
/* http://www.thegeekstuff.com/2012/03/catch-signals-sample-    */
/* c-code/                                                      */
/* =============================================================*/

void sigcap (int sig) {		
	// Nothing needed here.
}

/* =============================================================*/
/* Functions that deal with history! Uses a circular array.     */
/* =============================================================*/

void add_hist(char* entry) {
	char* inst;
	hist_pos = (hist_pos + 1) % MAX_HIST;

	if (!(inst = malloc(strlen(entry)+1))) {
		printf("Error: Cannot allocate memory for new string.");
	} else {
		strcpy(inst, entry);
	}

	if (hist[hist_pos]) {
		free(hist[hist_pos]);
	}

	hist[hist_pos] = inst;
}

/* =============================================================*/
/* Functions that parse / inspect the entry!                    */
/* =============================================================*/

int get_num_pipe(char* entry) {
	int i, num_pipe = 0;

	for (i = 0; i < strlen(entry); i++) {
		if (entry[i] == '|') {
			num_pipe++;
		}
	}
	return num_pipe;
}

void parse_entry(char* entry) {
	char* token;
	token = strtok(entry, "|\n");
	cmds[0] = token;

	int i = 1;
	while (token = strtok(NULL, "|\n")) {
		cmds[i] = token;
		i++;
	}
}

void parse_command(char* command) {
	char* token;
	token = strtok(command, " \t");
	args[0] = token;

	int i = 1;
	while (token = strtok(NULL, " \t")) {
		args[i] = token;
		i++;
	}
}

/* =============================================================*/
/* Clears the global arrays.                                    */
/* =============================================================*/

int clear_args() {
	if (!memset(args, '\0', sizeof(args))) {
		return 1;
	} return 0;
}

int clear_cmds() {
	if (!memset(cmds, '\0', sizeof(cmds))) {
		return 1;
	} return 0;
}

int clear_pids() {
	if (!memset(pids, '\0', sizeof(pids))) {
		return 1;
	} return 0;
}

int clear_hist() {
	int i;
	for (i = 0; i < MAX_HIST; i++) {
		if(hist[i]) {
			free(hist[i]);
		}
	}
}

/* =============================================================*/
/* Functions that deal with non-bin operations!                 */
/* =============================================================*/

void proc_exit() {
	clear_hist();
	exit(0);
}

void proc_pwd() {
	char buf[MAX_BUFF];

	if (!getcwd(buf, sizeof(buf))) {
		printf("Error: Directory is invalid!\n");
	} else {
		printf("%s\n\n", buf);
	}
}

void proc_cd() {
	if (!args[1]) {
		printf("Error: Argument for path is invalid!\n");
	} else {
		if (chdir(args[1])) {
			printf("Error: Path is invalid!\n");
		} else {
			printf("Changed directory to:\n");
			proc_pwd();
		}
	}
}

void proc_hist() {
	printf("Last %i inputted commands:\n", MAX_HIST);

	int i;
	for (i = 0; i < MAX_HIST; i++) {
		if (!hist[modn(hist_pos - i, MAX_HIST)]) { 
			break; 
		}

		printf("%i. %s", (i + 1), hist[modn(hist_pos - i, MAX_HIST)]);

	} printf("\n");
}

/* =============================================================*/
/* Functions that deal with processes and whatnot!              */
/* =============================================================*/

void wait_childs() {
	int stat, i;
	for (i = 0; i <= num_pipe; i++) {
		waitpid(pids[i], &stat, 0);
	}
}

void proc_op() {
	pid_t pid = fork();

	// Error case.
	if (pid < 0) {	
		printf("\nError: Forking problem.\n");

	// Child case.
	} else if (pid == 0) {
		if (execvp(args[0], args)) {
			printf("Error: Not a command.\n");
			exit(0);
		}
	
	// Parent case.
	} else {
		wait(NULL);
	}
}

void proc_op_in() {
	pipe(pipes);

	pid_t pid = fork();
	pids[pipe_pos] = pid;

	// Error case.
	if (pid < 0) {	
		printf("\nError: Forking problem.\n");

	// Child case.
	} else if (pid == 0) {
		close(STDOUT_FILENO);
		dup2(pipes[1], STDOUT_FILENO);

		execvp(args[0], args);
		printf("Error: Not a command.\n");
		exit(0);
		
	
	// Parent case.
	} else {
		close(pipes[1]);
	}
}

void proc_op_mid() {
	temp_pipe = pipes[0];
	pipe(pipes);

	pid_t pid = fork();
	pids[pipe_pos] = pid;

	// Error case.
	if (pid < 0) {	
		printf("\nError: Forking problem.\n");

	// Child case.
	} else if (pid == 0) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		dup2(temp_pipe, STDIN_FILENO);
		dup2(pipes[1], STDOUT_FILENO);

		execvp(args[0], args);
		printf("Error: Not a command.\n");
		exit(0);
		
	
	// Parent case.
	} else {
		close(temp_pipe);
		close(pipes[1]);
	}

}

void proc_op_out() {
	pid_t pid = fork();
	pids[pipe_pos] = pid;

	// Error case.
	if (pid < 0) {	
		printf("\nError: Forking problem.\n");

	// Child case.
	} else if (pid == 0) {
		close(STDIN_FILENO);
		dup2(pipes[0], STDIN_FILENO);

		execvp(args[0], args);
		printf("Error: Not a command.\n");
		exit(0);
		
	
	// Parent case.
	} else {
		close(pipes[0]);
	}
}

/* =============================================================*/
/* Inspects the command given!                                  */
/* =============================================================*/

int command_get() {
	if (!args[0]) {
		printf("ERROR: Arguments are invalid!\n");
	} else if (strcmp(args[0], "exit") == 0) {
		proc_exit();
	} else if (strcmp(args[0], "pwd") == 0) {
		proc_pwd();
	} else if (strcmp(args[0], "cd") == 0) {
		proc_cd();
	} else if (strcmp(args[0], "history") == 0) {
		proc_hist();
	} else {
		if (num_pipe == 0) {
			//printf("Executing a non-piped process...\n");
			proc_op();
		} else {
			// First pipe...
			if (pipe_pos == 0) {
				//printf("Executing first piped process...\n");
				proc_op_in();
				pipe_pos++;

			// Last pipe...	
			} else if (pipe_pos == num_pipe) {
				//printf("Executing last piped process...\n");
				proc_op_out();

				//printf("Waiting on child(ren)...\n");
				wait_childs();
				//printf("Children terminated successfully.\n");

			// Intermediate pipes...
			} else {
				//printf("Executing intermediate piped process...\n");
				pipe(pipes);
				proc_op_mid();
				pipe_pos++;
			}
		}
	}
}

/* =============================================================*/
/* Prints out things on start-up!                               */
/* =============================================================*/

void intro() {
	printf("CMPT 300 - Primitive Shell\n");
	printf("For Assignment 1\n");
	printf("by Timothy James Montesa (301261623)\n");
	printf("tmontesa@sfu.ca\n\n");
}

/* =============================================================*/
/* The main function!                                           */
/* =============================================================*/

int main() {
	signal(SIGINT, sigcap);	// Starts signal capturing.
	intro();	

	char* entry;
	while(1) {
		printf("[INPUT]: ");
		fgets(buff, MAX_BUFF, stdin);		
		entry = buff;				
		add_hist(entry);			

		num_pipe = get_num_pipe(entry);		
		parse_entry(entry);

		int c = 0;
		for (c = 0; c < (num_pipe + 1); c++) {
			parse_command(cmds[c]);
			command_get();
			clear_args();
		}

		clear_cmds();
		clear_pids();
		pipe_pos = 0;
		temp_pipe = -1;
		printf("\n");
	}

	return 0;	
}
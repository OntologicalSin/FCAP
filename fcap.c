/*
Author: Nick Lyubenko
Purpose: To interact and send commands to the server process through a command line interface
*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <arpa/inet.h> 
#include <unistd.h>  
#include <signal.h>
#include <sys/mman.h>

#define SHMSZ 27

const char* fcap;

/*Writes a string into shared memory*/
int write_shmem(char *buf)
{
	int shmid;
    key_t key;
    char *shm, *s;

    key = 6667; //key

    if ((shmid = shmget(key, SHMSZ, 0666)) < 0) { //locate segment
        perror("could not locate");
        exit(1);
    }
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {  //attach to data space
        perror("could not attach");
        exit(1);
    }
    
    s = shm;
    strcpy(s,buf);
    //printf("%s was copied into shared memory",(char*)shm);
}

/*spawns a process to execute a function*/
 int spawn(char* func, char** argl)
{
    int cid = fork();

	if(cid!=0){  //parent process knows child id
		return cid;
	}

	else //child process pid is zero
	{
		execvp(func, argl);
		int errcode = errno;
		fprintf(stderr, "Error reached: %s\n",strerror(errcode));
		abort();
	}

}

void print_usage(FILE* stream, int exit_code)
{
	fprintf(stream,
		"--start-server -s     <port>     Begin to accept connections from client machines\n"
		"--list-clients -l                List all client connections\n" 
		"--list-dir     -r   <dir>  <ip>  List files located specified machine\n" 
		"--file         -f   <path> <ip>  Specify location of a file to be captured\n"
		"--terminate    -x     <port>     Terminate server and reject clients\n"
		"--help         -h                List command line arguments\n"); 
	exit(exit_code);
}

/*Gets process ID of the process running on specified port*/
int getSID(char* port)
{
	char command[25], id[6];
	FILE *in = NULL;
	strcpy(command,"lsof -t -i:");
	strcat(command,port);
	printf("%s\n",command);
	in = popen(command,"r");
	fgets(id,6,in);
	printf("%s\n",id);
	return atoi(id);
}

int main (int argc, char* argv[])
{
	int next_option, server_id;
	char server_path[128], *line = NULL, temp[6], *choose_ip, dir[30];
	FILE *in = NULL, *out;
    size_t len = 0;
	ssize_t read;

	getcwd(server_path, sizeof(server_path));
	strcat(server_path,"/server");
	printf("%s\n",server_path );

	/* A string listing valid short options letters.  */
	const char* const short_options = "lhs:f:x:r:";
	/* An array describing valid long options.  */
	const struct option long_options[] = {
	{ "start-server", 1, NULL, 's' },
	{ "list-clients", 0, NULL, 'l' },
	{ "list-dir",     1, NULL, 'r' },
	{ "file",         1, NULL, 'f' },
	{"terminate",     1, NULL, 'x' },
	{ "help",         0, NULL, 'h' },
	{ NULL,           0, NULL, 0   } };
	
	fcap = argv[0];
	
	do{
		next_option = getopt_long (argc, argv, short_options,long_options, NULL);
		switch (next_option)
		{
			case 'h':   //get help
				print_usage (stdout, 0);
				break;

			case 's':  ; /* -s or --start-server */

				char* arglist[] = {"server",argv[2],NULL};
				server_id = spawn(server_path,arglist);
				break;

			case 'l':   /* -l or  --list-clients */

				printf("\nConnected Clients\t\n---------------------\n"); //open file pointer while line print
				out = fopen("output.txt", "r");
    			if (out == NULL)
        			exit(EXIT_FAILURE);
    			while ((read = getline(&line, &len, out)) != -1) 
        			printf("%s\n", line);
        		fclose(out);
    			if (line)
        			free(line);
				break;

			case 'r':   /* -r or  --list-dir */
	    		strcpy(dir, "r ");
				strcat(dir, (char*)argv[2]);
				//strcat(dir, (char*)argv[3]); //client ip
				write_shmem(dir); //-r /insert/dir/here/
				printf("\n%s\n",dir);
				kill(getSID("66"), SIGUSR1);
				printf("Recieved Directory:\n\n"); //signal to server to read shared memory
				break;

			case 'f':   /* -f or  --file */
	    		strcpy(dir, "f ");
				strcat(dir, (char*)argv[2]);
				//strcat(dir, (char*)argv[3]); //client ip
				write_shmem(dir); // -f /insert/file/here.exe
				printf("\n%s\n",dir);
				kill(getSID("66"), SIGUSR1);
				printf("File saved in current working directory\n");
				//signal to server to read shared memory
				break;
			case 'x':   /* -x or  --terminate */
				remove("output.txt");
				fprintf(stderr, "TERMINATING server %d\n", getSID(argv[2]));
				kill(getSID(argv[2]),SIGTERM);
				printf("SERVER TERMINATED\n");
				break;

			case '?':  
				print_usage(stderr, 1);
			case -1:    /* Done with options.  */
				break;
			default:    /* Something else: unexpected.  */
				abort ();
				}
			}
		while (next_option != -1);

	return 0;
}
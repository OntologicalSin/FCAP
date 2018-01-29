#include <stdio.h>
#include <string.h> 
#include <stdlib.h>  
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>  
#include <pthread.h> 
#include <fcntl.h>
#include <sys/stat.h>
#define SHMSZ     27
#define MAX_SEND_BUF 256
#define MAX_RECV_BUF 256

FILE *output;
char *fname = "output.txt", *shm, *cli_dir[1000];
sig_atomic_t sigusr1_count = 0;
int sock; 
void *sock_desc;

void* connection_handler(void* blah);
/*Recieves a file sent by client*/
int recv_file(char* file_name)
{
    printf("MAKING FILE CALLED %s\n",file_name);
    int f;
    ssize_t bytes, file_size;
    int count;
    char sent_string[MAX_RECV_BUF];

    if ( (f = open(file_name, O_WRONLY|O_CREAT, 0644)) < 0 )
    {
        perror("error creating file");
        return -1;
    }

    while ( (bytes = recv(sock, sent_string, MAX_RECV_BUF, 0)) > 0 )
    {
        if (write(f, sent_string, bytes) < 0 )
        {
            perror("error writing to file");
            return -1;
        }
    }
    close(f); //close file
    printf("Received");
    return 1;
}

int init() //create shared memory location
{
    char c;
    int shmid;
    key_t key;
    char *s;

    key = 6667;

    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }

    s = shm;

    sprintf(s,"Server accessed shared mem");
}

void* processClient(void *voidData)
{
    int read_size;
    char *message , client_message[2000];
    int i = 0;

    message = shm;
    write(sock , message , strlen(message));
    
    //sleep(1); //makes sure client processed command

    if(message[0]=='r')
    {
        while((read_size = recv(sock , client_message , 2000 , 0)) > 0 )
        {

            char* new;
            new = strtok(client_message,"^");
            printf("%s\n",new);
            i = 0;

            while (new != NULL)
            {
                cli_dir[i] = (char*)malloc(sizeof(char)* (strlen(new) + 1 ) );
                strncpy(cli_dir[i],new,strlen(new));
                printf("%s\n",cli_dir[i]);
                new = strtok (NULL, "^");
                ++i;
            }
        }
    }

    else if(message[0]=='f')
    {
        char* dirs[2];

        char* new = strtok (message,"."); 
        printf("%s\n",new);
        puts("starting while loop");
        int i = 0;
        while (new != NULL)
        {
            dirs[i]=new;
            printf("%s\n",dirs[i]);
            new = strtok (NULL, "."); //seperate name from exetention
            ++i;
        }

        printf("%s.%s\n\n",(char*)dirs[0],(char*)dirs[1]);  //print name
        char saved_dir[sizeof(dirs[0])+sizeof(dirs[1])+2]; //file.extention
        strcpy(saved_dir,"saved.");
        strcat(saved_dir,dirs[1]); //saved.*
        recv_file(saved_dir);
    }

    else if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
        perror("error 2 here"); 
    }
    else if(read_size == -1)
    {
        perror("recv server side failed");
    }

    memset(message, 0, sizeof(message));
    memset(client_message, 0, sizeof(client_message));
    memset(cli_dir, 0, sizeof(cli_dir));
    
}
/*creates thread to process client*/
void handler(int signal_number)
{   
    pthread_t sniffer_thread;
         
    if( pthread_create( &sniffer_thread , NULL , processClient ,NULL )< 0)
    {
        perror("could not create thread");
    }
}

void cleaner(int signal_number)
{

}
/*Listens for signals coming from fcap.c*/
void* connection_handler(void *socket_desc)
{

    sock_desc = socket_desc;
    puts("accessed connection handler");

    sock = *(int*)socket_desc;

    if(signal( SIGUSR1, handler ) == SIG_ERR ) {
      perror( "Error with signal" );
      exit(1);
   }
    if(signal( SIGTERM, cleaner ) == SIG_ERR ) {
      perror( "Error with signal" );
      exit(1);
   }

   while(1) {
      (void)sleep( 1 );
   }

   pause();
   connection_handler(socket_desc);

}

int main(int argc, char* argv[]) ///MUST ADD CLIENT CHOOSING INTERACTION!!!!!!!!!!!!!!!!
{   
    char clientName[INET_ADDRSTRLEN];
    int portno = atoi(argv[1]);
    int socket_desc , client_sock , c , *new_sock;
    struct sockaddr_in server , client;

    init();
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( portno );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    while(1){
    // //Listen
     listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    output = fopen(fname,"w");
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;
         
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
         
        puts("Handler assigned");

        if(inet_ntop(AF_INET,&client.sin_addr.s_addr,clientName,sizeof(clientName))!=NULL){
             fprintf(output,"%s%c%d",clientName,'/',ntohs(client.sin_port));  
             fclose(output);
        } 
        else {
             printf("Unable to get address\n");
        }

    }

    if (client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }
    }

    return 0;
}

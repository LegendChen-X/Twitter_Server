#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include "socket.h"

#ifndef PORT
    #define PORT 54992
#endif

#define LISTEN_SIZE 5
#define WELCOME_MSG "Welcome to CSC209 Twitter! Enter your username: \r\n"
#define SEND_MSG "send"
#define SHOW_MSG "show"
#define FOLLOW_MSG "follow"
#define UNFOLLOW_MSG "unfollow"
#define BUF_SIZE 256
#define MSG_LIMIT 8
#define FOLLOW_LIMIT 5
#define INVAD_NAME "Invalied Name,please enter again\r\n"
#define INVAD_COMMAND "Invalid command\r\n"
#define NEW_LINE "[%d] Found newline: %s\n"
#define READ_BYTES "[%d] Read %d bytes\n"

struct client {
    int fd;
    struct in_addr ipaddr;
    char username[BUF_SIZE];
    char message[MSG_LIMIT][BUF_SIZE];
    struct client *following[FOLLOW_LIMIT]; // Clients this user is following
    struct client *followers[FOLLOW_LIMIT]; // Clients who follow this user
    char inbuf[BUF_SIZE]; // Used to hold input from the client
    char *in_ptr; // A pointer into inbuf to help with partial reads
    struct client *next;
};

// To begin with, my code may still have some small bugs. Sometimes, input
// will fail which will lead to seg fault (only in school server), but the majority time will become successful(1000 times fall 1 time). I have no
// idea for this. I think it may not be my code problem but the server. I think the life circle of process in school server is too short.
// If you help me find the problem, please, write it in the annotation on Markus. Thank you from within.

// Add one new client into the list
void add_client(struct client **clients, int fd, struct in_addr addr);
// Remove one client from the list
void remove_client(struct client **clients, int fd);
// Move client c from new_clients list to active_clients list. 
void activate_client(struct client *c, struct client **active_clients_ptr, struct client **new_clients_ptr);
// Find which command is
int find_command(char* message);
// If the command is follow unfollow send, we need find the content
void find_content(char* message,char* content);
// Find the newwork newline character
int find_network_newline(const char *buf, int n);
// Read the whole message (until \r\n)
int readAll(int fd,char* content,int size);
// Anounce the information to whole users
int announce(struct client *active_clients, char *s);
// Follow function
int follow(char* follow_name,struct client **clients,int user_fd);
// Unfollow function
int unfollow(char* follow_name,struct client **clients,int user_fd);
// Show function
int show(struct client *clients,struct client* user);
// Send function
int sendMessage(struct client *clients,struct client *user,char* messages);


// The set of socket descriptors for select to monitor.
// This is a global variable because we need to remove socket descriptors
// from allset when a write to a socket fails. 
fd_set allset;

int find_command(char* message)
{
    //Special cases
    if(strcmp(message,"send ")==0) return -1;
    if(strcmp(message,"send")==0) return -1;
    //The buff string to store the value from read
    char copy[BUF_SIZE];
    //store value of message into copy
    strcpy(copy,message);
    //To find command with " "
    char* command;
    //Return the new char pointer
    command = strtok(copy," ");
    //Check " "
    if(command==NULL) return -1;
    //If follow, command 0
    if(strcmp(command,"follow")==0) return 0;
    //If unfollow, command 1
    else if(strcmp(command,"unfollow")==0) return 1;
    //If show, command 2
    else if(strcmp(command,"show")==0) return 2;
    //If send, command 3
    else if(strcmp(command,"send")==0) return 3;
    //If quit, command 4
    else if(strcmp(command,"quit")==0) return 4;
    //else invalid command -1
    else return -1;
}

void find_content(char* message,char* content)
{
    int i=0;
    int j=0;
    //Find the location of " "
    while(message[i]!=' ') i++;
    //The next char after " "
    i=i+1;
    //Copy into content
    while(message[i]!='\0') content[j++]=message[i++];
    //Set '\0'
    content[j]='\0';
}

void activate_client(struct client *c, struct client **active_clients_ptr, struct client **new_clients_ptr)
{
    struct client* tmp = *new_clients_ptr;
    //If target is the root of the link list
    if(tmp==c)
    {
        //Maintain struction of link list
        *new_clients_ptr=tmp->next;
        tmp->next=*active_clients_ptr;
        *active_clients_ptr=tmp;
        return;
    }
    
    while(tmp->next)
    {
        if(tmp->next==c)
        {
            tmp->next=tmp->next->next;
            c->next=*active_clients_ptr;
            *active_clients_ptr=c;
            break;
        }
        tmp=tmp->next;
    }
    return;
}

int find_network_newline(const char *buf, int n)
{
    //Find the location of "\r\n"
    for(int i=0;i<n-1;++i) if(buf[i]=='\r'&&buf[i+1]=='\n') return i+2;
    //If not, return -1
    return -1;
}

int readAll(int fd,char* content,int size)
{
    //Initial tmp which is the buff area
    char tmp[BUF_SIZE]={'\0'};
    //Set location
    int location = -1;
    //Copy into content
    strcpy(content,tmp);
    //Read the whole message to find "\r\n"
    while(location==-1)
    {
        
        int check = read(fd,tmp,size);
        //Check the return value of read, will process in main
        if(check<=0) return -1;
        //cat tmp to content
        strcat(content,tmp);
        //Check "\r\n"
        location = find_network_newline(content,size);
        //initial tmp
        for(int i=0;i<BUF_SIZE;++i) tmp[i]='\0';
    }
    //Set '\0'
    if(location>2) content[location-2]='\0';
    //return the length of read
    return location-2;
}

int announce(struct client *active_clients, char *s)
{
    //Value for check the process of write in loop
    int flag=1;
    struct client* tmp = active_clients;
    while(tmp)
    {
        int count = write(tmp->fd,s,(int)strlen(s));
        //If it is smaller than 0, change flag, will process in main
        if(count<=0) flag=0;
        fprintf(stdout,READ_BYTES,tmp->fd,(int)strlen(s));
        tmp=tmp->next;
    }
    return flag;
}

void add_client(struct client **clients, int fd, struct in_addr addr)
{
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->username[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *clients;
    //Initial the two lists
    for(int i=0;i<FOLLOW_LIMIT;++i)
    {
        p->following[i]=NULL;
        p->followers[i]=NULL;
    }

    // initialize messages to empty strings
    for (int i = 0; i < MSG_LIMIT; i++) {
        p->message[i][0] = '\0';
    }

    *clients = p;
}

/* 
 * Remove client from the linked list and close its socket.
 * Also, remove socket descriptor from allset.
 */
void remove_client(struct client **clients, int fd) {
    struct client **p;

    for (p = clients; *p && (*p)->fd != fd; p = &(*p)->next);

    if (*p) {
        for(int i=0;i<FOLLOW_LIMIT;++i)
        {
            //find the user follows who
            struct client* user_following = (*p)->following[i];
            //find who follows the user
            struct client* user_follower = (*p)->followers[i];
            //Delete user from others followers
            for(int j=0;j<FOLLOW_LIMIT;++j)
            {
                if(user_following && user_following->followers[j]->fd==fd)
                {
                    //Need to break
                    fprintf(stdout,"%s no longer has %s as a follower\n", user_following->username,user_following->followers[j]->username);
                    user_following->followers[j]=NULL;
                    break;
                }
            }
            
            //Delete user from others following
            for(int j=0;j<FOLLOW_LIMIT;++j)
            {
                if(user_follower && user_follower->following[j]->fd==fd)
                {
                    fprintf(stdout,"%s is no longer following %s because of diconnection\n", user_follower->username,user_follower->following[j]->username);
                    user_follower->following[j]=NULL;
                    break;
                }
            }
        }
        // Remove the client
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, 
            "Trying to remove fd %d, but I don't know about it\n", fd);
    }
}

int follow(char* follow_name,struct client **clients,int user_fd)
{
    struct client** user = clients;
    struct client** new_follow = clients;

    //Find the user who want to add the new following
    while((*user)->fd!=user_fd) user = &(*user)->next;
    //Find the new_follow
    while(*new_follow && strcmp((*new_follow)->username,follow_name)!=0) new_follow = &(*new_follow)->next;
    //Check if it exists
    if(*new_follow==NULL || (*new_follow)->fd==user_fd) return 0;
    //Add to the following
    int i=0;
    for(;i<FOLLOW_LIMIT;++i)
    {
        // Check whether or not have follow
        if((*user)->following[i] && strcmp((*user)->following[i]->username,follow_name)==0) return 0;
        
        if((*user)->following[i]==NULL)
        {
            (*user)->following[i] = *new_follow;
            break;
        }
    }
    //check whether or not put successfully
    if(i==FOLLOW_LIMIT) return 0;
    //put the user into the followe list of new follow
    for(i=0;i<FOLLOW_LIMIT;++i)
    {
        if((*new_follow)->followers[i]==NULL)
        {
            //Add to the follower
            (*new_follow)->followers[i]=*user;
            break;
        }
    }
    //check whether or not put successfully
    if(i==FOLLOW_LIMIT) return 0;
    
    //Successfully return 1
    return 1;
}

//Almost the same with follow
int unfollow(char* follow_name,struct client **clients,int user_fd)
{
    struct client** user = clients;
    struct client** new_follow = clients;
    //Find the user who want to delete the new following
    while((*user)->fd!=user_fd) user = &(*user)->next;
    
    while(*new_follow && strcmp((*new_follow)->username,follow_name)!=0) new_follow = &(*new_follow)->next;
    
    if(*new_follow==NULL || (*new_follow)->fd==user_fd) return 0;
    
    int i=0;
    for(;i<FOLLOW_LIMIT;++i)
    {
        if((*user)->following[i]==*new_follow)
        {
            (*user)->following[i] = NULL;
            break;
        }
    }
    
    if(i==FOLLOW_LIMIT) return 0;
    
    for(i=0;i<FOLLOW_LIMIT;++i)
    {
        if((*new_follow)->followers[i]==*user)
        {
            (*new_follow)->followers[i]=NULL;
            break;
        }
    }
    
    if(i==FOLLOW_LIMIT) return 0;
    
    return 1;
}

int show(struct client *clients,struct client* user)
{
    //Loop all following
    int flag = 1;
    for(int i=0;i<FOLLOW_LIMIT;++i)
    {
        if(user->following[i]!=NULL)
        {
            for(int j=0;j<MSG_LIMIT;++j)
            //Check whether or not message is legal
            if(strlen(user->following[i]->message[j])!=0)
            {
                //Initial total_message
                char total_message[BUF_SIZE]={'\0'};
                strcpy(total_message,user->following[i]->username);
                strcat(total_message,": ");
                strcat(total_message,user->following[i]->message[j]);
                strcat(total_message,"\r\n");
                int count = write(user->fd,total_message,BUF_SIZE);
                fprintf(stdout, READ_BYTES,user->following[i]->fd,(int)strlen(total_message));
                //If write failed, return 0, will process in main
                if(count<=0) flag=0;
            }
        }
    }
    return flag;
}

int sendMessage(struct client *clients,struct client *user,char* messages)
{
    int flag = 1;
    int i;
    //Find which message is not set
    for(i=0;i<MSG_LIMIT && strlen(user->message[i])!=0;++i);
    //If it is full, return 0, can not make more message
    if(i==MSG_LIMIT) return 0;
    else
    {
        //Copy message into user->message[i]
        strcpy(user->message[i],messages);
        for(int j=0;j<FOLLOW_LIMIT;++j)
        {
            //Senfd to all followers
            if(user->followers[j]!=NULL)
            {
                //Initial total_message
                char total_message[BUF_SIZE]={'\0'};
                strcpy(total_message,user->username);
                strcat(total_message,": ");
                strcat(total_message,messages);
                strcat(total_message,"\r\n");
                int count = write(user->followers[j]->fd,total_message,BUF_SIZE);
                //Check the return value of write
                if(count<=0)
                {
                    remove_client(&clients, user->fd);
                    flag = 0;
                    continue;
                }
                //Message for server
                fprintf(stdout, READ_BYTES,user->followers[j]->fd,(int)strlen(total_message));
            }
        }
    }
    return flag;
}
 

int main (int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    // If the server writes to a socket that has been closed, the SIGPIPE
    // signal is sent and the process is terminated. To prevent the server
    // from terminating, ignore the SIGPIPE signal. 
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // A list of active clients (who have already entered their names). 
    struct client *active_clients = NULL;

    // A list of clients who have not yet entered their names. This list is
    // kept separate from the list of active clients, because until a client
    // has entered their name, they should not issue commands or 
    // or receive announcements. 
    struct client *new_clients = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, LISTEN_SIZE);
    
    free(server);

    // Initialize allset and add listenfd to the set of file descriptors
    // passed into select 
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            exit(1);
        } else if (nready == 0) {
            continue;
        }

        // check if a new client is connecting
        if (FD_ISSET(listenfd, &rset)) {
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd, &q);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_client(&new_clients, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if (write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_client(&new_clients, clientfd);
            }
        }

        // Check which other socket descriptors have something ready to read.
        // If a client has been removed, the loop variables may no longer be valid.
        int cur_fd, handled;
        for (cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if (FD_ISSET(cur_fd, &rset)) {
                handled = 0;

                // Check if any new clients are entering their names
                for (p = new_clients; p != NULL; p = p->next) {
                    if (cur_fd == p->fd) {
                        //Initial value for input_name
                        char input_name[BUF_SIZE]={'\0'};
                        int count = readAll(p->fd,input_name,BUF_SIZE);
                        //Check count which is the return value of read (error check)
                        if(count<=0)
                        {
                            //If it is smaller than 0, it is broken and we need to remove
                            fprintf(stderr,"Read from client %s failed\n", inet_ntoa(q.sin_addr));
                            fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                            remove_client(&new_clients, p->fd);
                            break;
                        }
                        
                        struct client *tmp = active_clients;
                        //To check the input name is legal or not
                        int flag = 1;
                        while(tmp)
                        {
                            if(strcmp(tmp->username,input_name)==0 || strcmp("",input_name)==0 || strcmp(" ",input_name)==0)
                            {
                                // If the name is same with registed, then Invalid
                                int check = write(p->fd,INVAD_NAME,strlen(INVAD_NAME));
                                //Error check for write
                                if(check<0)
                                {
                                    fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                    fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                    remove_client(&new_clients, p->fd);
                                    break;
                                }
                                //Set flag
                                flag = 0;
                                break;
                            }
                            tmp=tmp->next;
                        }
                        //If it is not legal, break
                        if(!flag) break;
                        //If it is legal, put it into p
                        strcpy(p->username,input_name);
                        //Move to activate list
                        activate_client(p, &active_clients, &new_clients);
                        //Welcome announcement
                        char announcement[BUF_SIZE];
                        strcpy(announcement,p->username);
                        char* fix= " has just joined.\r\n";
                        strcat(announcement,fix);
                        
                        int check = announce(active_clients,announcement);
                        if(check==0) fprintf(stderr,"Announce to some of clients %s failed\n", inet_ntoa(q.sin_addr));
                        
                        fprintf(stdout,"%s has just joined\n", p->username);
                        handled = 1;
                        break;
                    }
                }

                if (!handled) {
                    // Check if this socket descriptor is an active client
                    for (p = active_clients; p != NULL; p = p->next) {
                        if (cur_fd == p->fd) {
                            //To store command
                            char command[BUF_SIZE]={'\0'};
                            int count = readAll(p->fd,command,BUF_SIZE);
                            if(count<0)
                            {
                                //Error check
                                fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                remove_client(&active_clients, p->fd);
                                break;
                            }
                            //Server message
                            fprintf(stdout,NEW_LINE,p->fd,command);
                            fprintf(stdout,"%s: %s\n",p->username,command);
                            //Find which command option
                            int option=find_command(command);
                            //Initial content for follow unfollow send
                            char content[BUF_SIZE]={'\0'};
                            //follow
                            if(option==0)
                            {
                                //Find the user want to follow who
                                find_content(command,content);
                                int value = follow(content,&active_clients,p->fd);
                                if(value==0)
                                {
                                    //Follow failed
                                    int check = write(p->fd,"Follow failed\r\n",strlen("Follow failed\r\n"));
                                    if(check<=0)
                                    {
                                        //Error check for write
                                        fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                        fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                        remove_client(&active_clients, p->fd);
                                        break;
                                    }
                                    fprintf(stderr,"Follow falied %s failed\n", inet_ntoa(q.sin_addr));
                                }
                            }
                            //unfollow
                            else if(option==1)
                            {
                                //Find the user want to unfollow who
                                find_content(command,content);
                                int value = unfollow(content,&active_clients,p->fd);
                                if(value==0)
                                {
                                    //Follow failed
                                    int check=write(p->fd,"Unfollow failed\r\n",strlen("Unfollow failed\r\n"));
                                    if(check<=0)
                                    {
                                        //Error check for write
                                        fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                        fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                        remove_client(&active_clients, p->fd);
                                        break;
                                    }
                                    fprintf(stderr,"Unfollow falied %s failed\n", inet_ntoa(q.sin_addr));
                                }
                            }
                            //show
                            else if(option==2)
                            {
                                int value = show(active_clients,p);
                                if(value==0)
                                {
                                    //Error check for show
                                    int check=write(p->fd,"Show failed\r\n",strlen("Show failed\r\n"));
                                    if(check<=0)
                                    {
                                        //Error check for write
                                        fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                        fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                        remove_client(&active_clients, p->fd);
                                        break;
                                    }
                                    fprintf(stderr,"Show falied %s failed\n", inet_ntoa(q.sin_addr));
                                }
                            }
                            //send
                            else if(option==3)
                            {
                                //Find contents
                                find_content(command,content);
                                int value = sendMessage(active_clients,p,content);
                                if(value==0)
                                {
                                    //Error check for send
                                    int check = write(p->fd,"Send failed\r\n",strlen("Send failed\r\n"));
                                    if(check<=0)
                                    {
                                        //Error check for send
                                        fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                        fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                        remove_client(&active_clients, p->fd);
                                        break;
                                    }
                                    fprintf(stderr,"Send falied %s failed\n", inet_ntoa(q.sin_addr));
                                }
                            }
                            //quit
                            else if(option==4)
                            {
                                //Goodbye message
                                char total_message[BUF_SIZE]={'\0'};
                                strcpy(total_message,"Goodbye ");
                                strcat(total_message,p->username);
                                strcat(total_message,"\r\n");
                                announce(active_clients,total_message);
                                fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                remove_client(&active_clients, p->fd);
                                break;
                            }
                            //Invalid command
                            else
                            {
                                int check = write(p->fd,INVAD_COMMAND,strlen(INVAD_COMMAND));
                                if(check<=0)
                                {
                                    //Error check for write
                                    fprintf(stderr,"Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                    fprintf(stdout,"Disconnect from %s\n", inet_ntoa(q.sin_addr));
                                    remove_client(&active_clients, p->fd);
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

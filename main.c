#include <stdio.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>

#define PORT 8080
#define TEXT_HTML 0
#define IMAGE_JPG 1
#define IMAGE_JPEG 2
#define IMAGE_PNG 3
#define AUDIO_MPEG 4
#define VIDEO_MP4 5
#define TEXT_PLAIN 6

char *content_types[] = {"text/html","image/jpg","image/jpeg","image/png","audio/mpeg","video/mp4","text/plain"};

int server_fd;
int running=1;
void* handle_client(void* new_socket);

char hex_to_char(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(const char *src, char *dest) {
    char ch;
    while (*src) {
        if (*src == '%') {
            if (isxdigit(*(src + 1)) && isxdigit(*(src + 2))) {
                ch = (hex_to_char(*(src + 1)) << 4) | hex_to_char(*(src + 2));
                *dest++ = ch;
                src += 3;
            } else {
                *dest++ = *src++;
            }
        } else if (*src == '+') {
            *dest++ = ' ';
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}


char *get_file(char *data){
    char *token = strtok(data,"\n");
    token = strtok(token," ");
    int pos=1;
    while(pos < 2){
        token = strtok(NULL," ");
        pos++;
    }
    int length=strlen(token);
    printf("File: %s\n",token);
    for(int i=0;i<length-1;i++){
        token[i]=token[i+1];
    }
    token[length-1]='\0';
    return token;
}

int get_type(char *file){
    char *token = strtok(file,".");
    int pos=1;
    while(pos<2){
        token = strtok(NULL,".");
        pos++;
    }
    if (token == NULL) return TEXT_HTML;
    if (strncmp(token,"html",4) == 0){
        return TEXT_HTML;
    }
    else if (strncmp(token,"jpg",3) == 0){
        return IMAGE_JPG;
    }
    else if (strncmp(token,"jpeg",4) == 0){
        return IMAGE_JPEG;
    }
    else if (strncmp(token,"png",3) == 0){
        return IMAGE_PNG;
    }
    else if (strncmp(token,"mp3",3) == 0){
        return AUDIO_MPEG;
    }
    else if (strncmp(token,"mp4",3)==0){
        return VIDEO_MP4;
    }
    return TEXT_PLAIN;
}

void accept_client(struct sockaddr_in *address, socklen_t *addrlen){
    int new_socket;
    if ((new_socket=accept(server_fd,(struct sockaddr*)address,addrlen))<0){
        perror("accept");
        exit(EXIT_FAILURE);
    }
    pthread_t thread1;
    pthread_create(&thread1,NULL,handle_client,(void*)&new_socket);
    pthread_join(thread1,NULL);
}
void* handle_client(void *new_socket){
    printf("thread is running..\n");
    char buffer[1024] = {0};
    char *file;
    char file_decoded[256];
    char file_str[1024]={0};
    FILE *fptr;
    size_t bytes_read;
    char header[1024];
    int socket = (int)*(int*)new_socket;
    ssize_t valread = read(socket,buffer,1024-1);
    printf("Buffer: %s\n",buffer);
    printf("%ld\n",valread);
    file=get_file(buffer);
    url_decode(file,file_decoded);
    printf("url decoded: %s\n",file_decoded);
    DIR *dp;
    struct dirent *ep;
    dp = opendir(file_decoded);
    if (dp != NULL){
        snprintf(header,sizeof(header),"HTTP/1.1 200 OK\nContent-type:text/html\n\n");
        send(socket,header,strlen(header),0);
        while(ep=readdir(dp)){
            memset(buffer,0,sizeof(buffer));
            snprintf(buffer,sizeof(buffer),"<a href='/%s/%s'>%s</a><br>",file_decoded,ep->d_name,ep->d_name);
            send(socket,buffer,sizeof(buffer),0);
        }
        (void)closedir(dp);
        close(socket);
        return NULL;
    }

    if (strlen(file_decoded)==0){
        fptr = fopen("index.html","rb");
    }
    else{
        fptr = fopen(file_decoded,"rb");
    }

    if (fptr != NULL){
        fseek(fptr, 0, SEEK_END);
        long file_size = ftell(fptr);
        rewind(fptr);

        //printf("Content type: %s\n",content_types[get_type(file)]);
        snprintf(header, sizeof(header),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: %s\r\n"
         "Content-Length: %ld\r\n"
         "Connection: close\r\n"
         "\r\n",
         content_types[get_type(file_decoded)],
         file_size);

        send(socket, header, strlen(header), 0);


        while ((bytes_read = fread(file_str,1,sizeof(file_str),fptr)) > 0) {
            send(socket, file_str, bytes_read, 0); 
        }
        fclose(fptr);
    } else {
        snprintf(header, sizeof(header), "HTTP/1.1 404 NOT FOUND\nContent-Type:text/html\n\n <h1> NOT FOUND!!!! </h1>");
        send(socket, header, strlen(header), 0);
    }
    close(socket);
    return NULL;
}

int main(int argc, char **argv){
    struct sockaddr_in address;
    int opt=1;
    socklen_t addrlen = sizeof(address);
    signal(SIGPIPE, SIG_IGN);
    if ((server_fd = socket(AF_INET,SOCK_STREAM,0))<0){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if (bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd,3)<0){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    /*valread = read(new_socket,buffer,1024-1);
    printf("%s\n",buffer);*/
    while(running) accept_client(&address,&addrlen);
    printf("Hello message sent\n");
    //close(new_socket);
    close(server_fd);
    return 0;
}

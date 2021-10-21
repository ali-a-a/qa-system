#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>


#define TIMEOUT 60

void alarm_handler(int signum) {
    write(1, "10s passed without input!\n", 26);
}

char* itoa(int num, char* str) {
    char digits[] = "0123456789";
    char temp[16];
    int i = 0, j = 0;
    do {
        temp[i++] = digits[num % 10];
        num /= 10;
    } while (num > 0);
    
    while (--i >= 0)
        str[j++] = temp[i];
    str[j] = '\0';

    return str;
}


int main(int argc, char const *argv[]) {
    int sock, broadcast = 1, opt = 1, group, port, id, turn = 0, n, is_sent_question_to = 0, len_ans = 0;
    char temp[100] = {0};
    char qa[2048] = {0};
    char first_ans[100] = {0};
    char second_ans[100] = {0};
    char buffer[1024] = {0};
    struct sockaddr_in bc_address, server_address;

    siginterrupt(SIGALRM, 1);
    signal(SIGALRM, alarm_handler); 

    write(1, "Select group. 1 is computer, 2 is architecture, 3 is mechanic and 4 is electric\n", 80);
    do {
        read(0, temp, 100);
        group = atoi(strtok(temp, "\n"));
        if (group < 1 || group > 4) {
            write(1, "Invalid group\n", 14);
            continue;
        }
        break;
    } while (1);

    write(1, "Group Specified\n", 16);

    sock = socket(AF_INET, SOCK_STREAM, 0);
   
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(atoi(argv[1])); 
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr *)&server_address, sizeof(server_address));
    itoa(group, temp);
    send(sock , temp , strlen(temp) , 0); 
    write(1, "Connected to Server\n", 21);
    write(1, "Waiting for other users...\n", 27);
    recv(sock, buffer, 1024, 0);
    char* port_str = strtok(buffer, "$");
    char* id_str = strtok(NULL, "$");
    port = atoi(port_str);
    id = atoi(id_str);
    write(1, "Port ", 5);
    write(1, port_str, strlen(port_str));
    write(1, " Received, your ID is ", 22);
    write(1, id_str, strlen(id_str));
    write(1, "!\n", 2);
    close(sock);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    bc_address.sin_family = AF_INET; 
    bc_address.sin_port = htons(port); 
    bc_address.sin_addr.s_addr = inet_addr("255.255.255.255");

    bind(sock, (struct sockaddr *)&bc_address, sizeof(bc_address));
    

    char *hello = "Hello from client";
    char *helloFromServer = "Hello from server";
    socklen_t len;
    len = sizeof(bc_address);

    int number_of_recv = 0;

    while(1) {
       if (id == 0) {
           if(!is_sent_question_to) {
                write(1, "Ask a question\n", 15);
                read(0, buffer, 1024);
                strcat(qa, "$");
                strcat(qa, strtok(buffer, "\n"));
                strcat(buffer, "$");
                strcat(buffer, itoa(id+1, temp));
                strcat(buffer, "$");
                sendto(sock, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *) &bc_address, sizeof(bc_address));
                write(1, "Question sent to everyone.\n", 27);
                is_sent_question_to = 1;
           } else {
               if(number_of_recv == 0){
                   recv(sock, buffer, 1024, 0);
                   number_of_recv++;
                   continue;
               }
               write(1, "Waiting for response...\n", 24);
               recv(sock, temp, 100, 0);
               write(1, "Response received: ", 19);
               char* q = strtok(temp, "$");
               char* turn = strtok(NULL, "$");
               int turn_int = atoi(turn);
               if(len_ans == 0) {
                   strcat(first_ans, temp);
                   len_ans++;
               } else if (len_ans == 1) {
                   strcat(second_ans, temp);
                   len_ans++;
               }
               write(1, temp, strlen(temp));
               write(1, " \n", 2);
               if(turn_int == id + 3) {
                   write(1, "Done!\n", 6);
                   write(1, "Select best answer\n", 19);
                   read(0, temp, 100);
                   int choice = atoi(strtok(temp, "\n"));
                   strcat(qa, " ");
                   if (choice == 1) {
                       strcat(qa, strtok(first_ans, "\n"));
                   } else {
                       strcat(qa, strtok(second_ans, "\n"));
                   }
                   sock = socket(AF_INET, SOCK_STREAM, 0);
   
                   server_address.sin_family = AF_INET; 
                   server_address.sin_port = htons(atoi(argv[1])); 
                   server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

                   connect(sock, (struct sockaddr *)&server_address, sizeof(server_address));
                   write(1, qa, 2048);
                   send(sock , qa , strlen(qa) , 0);
                   close(sock);
                   break;
               }
           }
        } else {
            recv(sock, buffer, 1024, 0);
            write(1, buffer, strlen(buffer));
            write(1, " \n", 2);
            char* q = strtok(buffer, "$");
            char* turn = strtok(NULL, "$");
            int turn_int = atoi(turn);
            if(turn_int == id) {
                write(1, "Answer the question\n", 20);
                write(1, "Timeout is ", 11);
                itoa(TIMEOUT, temp);
                write(1, temp, strlen(temp));
                write(1, " \n", 2);
                alarm(TIMEOUT);
                read(0, buffer, 1024);
                alarm(0);
                strcat(buffer, "$");
                strcat(buffer, itoa(id+1, temp));
                strcat(buffer, "$");
                sendto(sock, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *) &bc_address, sizeof(bc_address));
                is_sent_question_to = 2;
                break;
            }
            
        }
    }

    write(1, "\nThe end \n", 10);
    close(sock);

    return 0;
}
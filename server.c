#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>

#define MAX_CAP 3

int list_len = 0;

typedef struct {
    int users[MAX_CAP];
    int group_type;
    int len;
} waiting_list;


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


int generate_random_port() {
    struct sockaddr_in addr;
    int test_sock;
    addr.sin_port = htons(0); 
    addr.sin_family = AF_INET; 
    test_sock = socket(AF_INET, SOCK_DGRAM, 0);
    while(addr.sin_port == 0) {
        bind(test_sock, (struct sockaddr *)&addr, sizeof(addr));
        socklen_t len = sizeof(addr);
        getsockname(test_sock, (struct sockaddr *)&addr, &len);
    }
    close(test_sock);
    return addr.sin_port;
}

int add_user(int sock, int group, waiting_list list[], int* users_list_index) {
    int i, j;
    char temp[16];
    int* groups;
    int is_full = 0;
    int found = 0;
    for(j = 0; j < list_len; j++) {
        if(list[j].group_type == group && list[j].len < MAX_CAP) {
            list[j].users[list[j].len] = sock;
            list[j].len++;
            if(list[j].len == MAX_CAP) {
                is_full = 1;
            }
            *users_list_index = j;
            found = 1;
            list_len++;
            break;
        }
    }

    if (found == 0) {
        list[list_len].group_type = group;
        list[list_len].len = 1;
        list[list_len].users[0] = sock;
        list_len++;
    }


    itoa(group, temp);
    write(1, "New user added to waiting list for ", 35);
    write(1, "group ", 6);
    write(1, temp, strlen(temp));
    write(1, " \n", 2);
    
    return is_full;
}

void send_port(int group, waiting_list list[100], int users_list_index) {
    int port = generate_random_port();
    char buf[128] = {0};
    char* temp[20] = {0};

    for (int i = 0; i < 3; i++){
        itoa(port, buf);
        strcat(buf, "$");
        strcat(buf, itoa(i, temp));
        strcat(buf, "$");
        send(list[users_list_index].users[i], buf, strlen(buf), 0);
        close(list[users_list_index].users[i]);
        list[users_list_index].users[i] = 0;
        list[users_list_index].len = 0;
        list[users_list_index].group_type = 0;
    }
    itoa(port, temp);
    write(1, "Group is full, port ", 20);
    write(1, temp, strlen(temp));
    write(1, " sent to users\n", 15);
}


int main(int argc, char const *argv[]) {
    int server_fd, new_socket, valread, max_sd, port;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char temp[128] = {0};
    char* port_str[20] = {0};
    int users_list_index;
    int iter;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fd_set master_set, slave_set;
    waiting_list users_list[100];

    for(iter = 0; iter < 100; iter++) {
        users_list[iter].len = 0;
    }
       
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    opt = fcntl(server_fd, F_GETFL);
    opt = (opt | O_NONBLOCK);
    fcntl(server_fd, F_SETFL, opt);

    if(argc > 1) 
        port = atoi(argv[1]);
    else 
        port = 8080;

    itoa(port, port_str);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
       
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 4);

    FD_ZERO(&master_set);
    max_sd = server_fd;
    FD_SET(server_fd, &master_set);

    write(1, "Server is running on port ", 26);
    write(1, port_str, strlen(port_str));
    write(1, "\nWaiting for players...\n", 24);

    int file_fd;
    file_fd = open("file.txt", O_APPEND | O_RDWR | O_CREAT, 0666);


    while(1) {
        slave_set = master_set;
        select(max_sd + 1, &slave_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_sd; i++) {
            if (FD_ISSET(i, &slave_set)) {
                if (i == server_fd) {
                    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                    FD_SET(new_socket, &master_set);
                    if(new_socket > max_sd)
                        max_sd = new_socket;
                    write(1, "New user connected\n", 19);
                } else {
                    valread = recv(i , buffer, 1024, 0);
                    if(buffer[0] == '$') {
                        write(file_fd, buffer, strlen(buffer));
                        write(file_fd, " \n", 2);
                        write(1, "New question and answer appended to the file\n", 45);
                        memset(buffer, 0, sizeof buffer);
                    } else {
                        int group = atoi(buffer);
                        if(add_user(i, group, users_list, &users_list_index))
                            send_port(group, users_list, users_list_index);
                    }
                    FD_CLR(i, &master_set);
                }
            }
        }
    }

    close(file_fd);

    return 0; 
} 
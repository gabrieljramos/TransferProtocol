#include "game.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <sys/time.h> // para timeout
#include <sys/stat.h>
#include <stdint.h>
#include <sys/statvfs.h>

int server = 0;

int cria_raw_socket(const char *interface) {
    interface = "enp7s0f0";

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_ll sa = {0};
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = ifr.ifr_ifindex;

    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void faz_msg (unsigned char*msg){
    for (int i = 0; i < 30; i++){
        if (i % 2 == 0)
            msg[i] = 1;
        else
            msg[i] = 0;
    }
}

int main(){
    int sockfd = cria_raw_socket(NULL);
    if (!server) {
    
        unsigned char* msg;

        faz_msg (msg);

        send(sockfd, msg, sizeof(msg), 0);

    }
    else {
        unsigned char buffer[1024];
        while (1) {
            int n = recv(sockfd, buffer, sizeof(buffer),0);
            if (n > 29)
                printf("Tam Recebido %d\n", n);
        }
    }

}
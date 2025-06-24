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

int cria_raw_socket(char* nome_interface_rede) {
    nome_interface_rede = "enp7s0f0";
    // Cria arquivo para o socket sem qualquer protocolo
    int soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (soquete == -1) {
        fprintf(stderr, "Erro ao criar socket: Verifique se você é root!\n");
        exit(-1);
    }
 
    int ifindex = if_nametoindex(nome_interface_rede);
 
    struct sockaddr_ll endereco = {0};
    endereco.sll_family = AF_PACKET;
    endereco.sll_protocol = htons(ETH_P_ALL);
    endereco.sll_ifindex = ifindex;
    // Inicializa socket
    if (bind(soquete, (struct sockaddr*) &endereco, sizeof(endereco)) == -1) {
        fprintf(stderr, "Erro ao fazer bind no socket\n");
        exit(-1);
    }
 
    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    // Não joga fora o que identifica como lixo: Modo promíscuo
    if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        fprintf(stderr, "Erro ao fazer setsockopt: "
            "Verifique se a interface de rede foi especificada corretamente.\n");
        exit(-1);
    }
 
    return soquete;
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
        while (1) {
            unsigned char* msg = "Hello world!aa";
            //faz_msg (msg);
            send(sockfd, msg, strlen(msg)+1, 0);
        }
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
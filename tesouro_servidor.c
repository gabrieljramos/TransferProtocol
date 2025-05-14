#include "queue.h"
#include "game.h"
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

#define MARCADOR 0x7E
#define MAX_DADOS 127

unsigned char *extensoes[] = {".txt", ".mp4", ".jpeg"};
coord_t current_pos;

typedef struct {
    unsigned char marcador;
    unsigned char tamanho;
    unsigned char seq;
    unsigned char tipo;
    unsigned char checksum;
    unsigned char dados[MAX_DADOS];
} __attribute__((packed)) Frame;

int cria_raw_socket(const char *interface) {
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

unsigned char calcula_checksum(Frame *f) {
    unsigned char sum = 0;
    sum += f->tamanho;
    sum += f->seq;
    sum += f->tipo;
    for (int i = 0; i < f->tamanho; ++i) {
        sum += f->dados[i];
    }
    return sum;
}

void monta_frame(Frame *f, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam) {
    f->marcador = MARCADOR;
    f->seq = seq & 0x1F;
    f->tipo = tipo & 0x0F;
    if (tipo == 0 || tipo == 1 || (tipo >= 10 && tipo <= 13)) {
        f->tamanho = 0;
        memset(f->dados, 0, sizeof(f->dados));
    } else {
        if (tam > 127) tam = 127;
        f->tamanho = tam;
        memcpy(f->dados, dados, tam);
    }
    f->checksum = calcula_checksum(f);
}

//Busca um arquivo baseado nos tipos nas extencoes
int find_file(int file_num, unsigned char *file_name) {

    for (int i = 0; i < 3; i++) {
        snprintf(file_name, sizeof(file_name), "%d%s", file_num, extensoes[i]);
        FILE *fp = fopen(file_name, "rb");
        if (fp) {
            printf("Arquivo encontrado: %sTamanho: %ld\n", file_name, strlen(file_name));
            fclose(fp);
            return i;
        }
    }
    return -1;
}

//Envia ACK/NACK/OK+ACK
void envia_resposta(int sockfd, unsigned char seq, unsigned char tipo, struct sockaddr_ll* origem, unsigned char *msg) {

    Frame resposta;

    if (msg == NULL)
        monta_frame(&resposta, seq, tipo, NULL, 0);
    else
        monta_frame(&resposta, seq, tipo, msg, sizeof(msg));

    //sendto(sockfd, &resposta, sizeof(resposta), 0, (struct sockaddr*)&dst, sizeof(dst));
    send(sockfd, &resposta, sizeof(resposta), 0);
}

void escuta_mensagem(int sockfd, tes_t* tesouros) {
    unsigned char buffer[2048];
    while (1) {
        struct sockaddr_ll addr;
        socklen_t addrlen = sizeof(addr);
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
        if (bytes < 5) 
            continue;

        for (ssize_t i = 0; i < bytes - 4; ) { // sem ++ aqui
            if (buffer[i] == MARCADOR) {
                Frame *f = (Frame *)&buffer[i];
                unsigned char expected_checksum = calcula_checksum(f);

                if (f->checksum != expected_checksum) {
                    printf("[Servidor] Checksum inválido para seq=%u\n", f->seq);
                    envia_resposta(sockfd, f->seq, 1, &addr, NULL); // Envia NACK
                    i += sizeof(Frame);
                    continue;
                }
                unsigned char tipo = f->tipo & 0x0F;

                //Ignorar ACK/NACK/OK+ACK recebidos
                if (tipo == 0 || tipo == 1 || tipo == 2) {
                    printf("[Servidor] Ignorado frame de controle tipo=%d\n", tipo);
                    i += sizeof(Frame);
                    continue;
                }

                //Baseado no tipo da msg recebida, move o personagem
                if (tipo == 11) 
                    update_x('-', &current_pos);
                else if (tipo == 13) 
                    update_y('-',&current_pos);
                else if (tipo == 12) 
                    update_x('+', &current_pos);
                else if (tipo == 10) 
                    update_y('+',&current_pos);

                //Aloca memoria p/ nova posicao e salva ela na lista 
                move_t* new_move = malloc(sizeof(move_t));
                new_move->pos.x = current_pos.x;
                new_move->pos.y = current_pos.y;
                new_move->next = NULL;
                new_move->prev = NULL;
                add_move(new_move);

                printf("[Servidor] Posição (%d, %d) salva\n", new_move->pos.x, new_move->pos.y);

                int tesouro;
                unsigned char file_name[64];
                //Verifica se encontrou um tesouro e devolve seu id, caso contrario, -1
                if ((tesouro = treasure_found(tesouros,current_pos.x,current_pos.y)) >= 0) {
                    envia_resposta(sockfd, f->seq, 2, &addr, NULL); // Envia OK + ACK
                    int file_type = find_file(tesouros[tesouro].id, file_name); //Procura, na memoria, o tesouro encontrado
                    if (file_type == -1) { //Caso nao tenha encontrado
                        printf("Error: file not found\n");
                        return;
                    }
                    else {
                        envia_resposta(sockfd, f->seq, file_type+6 , &addr, file_name); //Caso tenha encontrado
                    }
                }
                else
                    envia_resposta(sockfd, f->seq, 0, &addr, NULL); // Envia ACK

                i += sizeof(Frame); // pular o frame que acabou de processar
            } else {
                i++; // só avança se não achou MARCADOR
            }
        }   
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    //Aloca um vetor p/ tesouros e inicia o jogo
    tes_t* tesouros = (tes_t*) malloc(sizeof(tes_t) * 8);
    tesouros = game_start();

    //Imprime lista de tesouros (para teste apenas)
    printf("Tesouros:\n");
    for (int i = 0; i < 8; i++) {
        printf("(%d,%d)\n", tesouros[i].pos.x,tesouros[i].pos.y);
    }

    //Salva a posicao inicial
    current_pos.x = 7;
    current_pos.y = 0;

    //Salva a interface e inicia o socket
    const char* interface = argv[1];
    int sockfd = cria_raw_socket(interface);

    //Comeca a esperar por msgs
    escuta_mensagem(sockfd,tesouros);

    close(sockfd);
    return 0;
}


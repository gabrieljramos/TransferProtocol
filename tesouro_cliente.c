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
#include <sys/time.h> // para timeout

#define MARCADOR 0x7E
#define MAX_DADOS 127
#define TIMEOUT_MILLIS 300
#define MAX_RETRANSMISSIONS 5


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

//Faz a soma de todos os campos necessarios e retorna seu valor
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

//Monta a struct do protocolo
void monta_frame(Frame *f, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam) {
    f->marcador = MARCADOR;
    f->seq = seq & 0x1F;
    f->tipo = tipo & 0X0F;
    if (tipo == 0 || tipo == 1 || (tipo >= 10 && tipo <= 13)) { //Tipos que nao possuem dados
        f->tamanho = 0;
        memset(f->dados, 0, sizeof(f->dados));
    } else { //Caso tam > 127 trunca tam
        if (tam > 127) tam = 127;
        f->tamanho = tam;
        memcpy(f->dados, dados, tam);
    }
    f->checksum = calcula_checksum(f);
}

//Loop p/ aguardar a espera de um ACK/NACK/OK+NACK
int espera_ack(int sockfd, unsigned char seq_esperado) {
   
    //Define timeout
    const int timeoutMillis = TIMEOUT_MILLIS;
    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    Frame resposta;
    while (1) {
        ssize_t n = recv(sockfd, &resposta, sizeof(resposta), 0);
        if (n > 0 && resposta.marcador == MARCADOR) {
            printf("[Cliente] Recebido %zd bytes\n", n);
            printf("[Cliente] Dados recebidos: ");
            for (int i = 0; i < n; i++) printf("%02X ", ((unsigned char*)&resposta)[i]);
            printf("\n");

            if (resposta.tipo == 0 && resposta.seq == seq_esperado) { //ACK
                printf("[Cliente] ACK recebido para seq=%u\n", seq_esperado);
                return 1;
            } else if (resposta.tipo == 1 && resposta.seq == seq_esperado) { //NACK
                printf("[Cliente] NACK recebido para seq=%u\n", seq_esperado);
                return -1;
            } else if (resposta.tipo == 2 && resposta.seq == seq_esperado) { // OK + ACK
                printf("[Cliente] Tesouro recebido para seq=%u\n", seq_esperado);
                return 2;
            } else if (resposta.tipo == 6 && resposta.seq == seq_esperado) { // OK + ACK
                printf("[Cliente] Texto recebido para seq=%u\n", seq_esperado);
                return 2;
            } 
            else {
                printf("[Cliente] Frame ignorado (tipo=%u, seq=%u)\n", resposta.tipo, resposta.seq);
                continue;
            }
        } else {
            printf("[Cliente] Timeout esperando ACK/NACK para seq=%u\n", seq_esperado);
            return 0;
        }
    }
}

//Funcao que faz o envio de mensagens
void envia_mensagem(int sockfd, const char *interface, unsigned char seq, unsigned char tipo) {
    Frame f;
    memset(&f, 0, sizeof(Frame));
    monta_frame(&f, seq, tipo, NULL, tipo);

    /*struct sockaddr_ll dst = {0};
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFINDEX, &ifr);

    dst.sll_ifindex = ifr.ifr_ifindex;
    dst.sll_halen = 6;
    dst.sll_addr[0] = 0xFF; // MAC broadcast
    dst.sll_addr[1] = 0xFF;
    dst.sll_addr[2] = 0xFF;
    dst.sll_addr[3] = 0xFF;
    dst.sll_addr[4] = 0xFF;
    dst.sll_addr[5] = 0xFF;*/

    int tentativas = 0;
    int ack_ok = 0;
    while (tentativas < MAX_RETRANSMISSIONS && !ack_ok) {
        //int bytes = sendto(sockfd, &f, sizeof(Frame), 0, (struct sockaddr *)&dst, sizeof(dst));
        int bytes = send(sockfd, &f, sizeof(Frame), 0);
        if (bytes < 0) perror("sendto");
        else printf("[Cliente] Frame enviado (seq=%u)\n", seq);

        int resultado = espera_ack(sockfd, seq);
        if (resultado == 1) //Caso o ack tenha chego com sucesso
            ack_ok = 1;
        else if (resultado == 2) {
            ack_ok = 1;
            resultado = espera_ack(sockfd, seq);
        }

        else //timeout/NACK
            tentativas++;
    }

    if (!ack_ok) { //Caso num de tentativa tenha excedido o maximo
        printf("[Cliente] Falha após %d tentativas\n", MAX_RETRANSMISSIONS);
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    //Salva a posicao atual
    coord_t current_pos;
    current_pos.x = 7;
    current_pos.y = 0;

    //Salva a interface e inicia o socket
    const char *interface = argv[1];
    int sockfd = cria_raw_socket(interface);

    //Inicializa a sequencia
    int seq = 0;

    //Cria e inicializa a matriz do mapa
    char map[8][8];
    start_map(map);

    //Imprime o mapa
    print_map(map);

    //Variavel para salvar o movimento escolhido
    int game_state;

    //Loop principal do jogo
    while (game_state != QUIT) {
        game_state = menu();
        switch(game_state) {
            case UP:
                envia_mensagem(sockfd, interface, seq, 11);
                update_x('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case LEFT:
                envia_mensagem(sockfd, interface, seq, 13);
                update_y('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case DOWN:
                envia_mensagem(sockfd, interface, seq, 12);
                update_x('+',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case RIGHT:
                envia_mensagem(sockfd, interface, seq, 10);
                update_y('+',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case QUIT:
                break;
            default:
                printf("Digite uma opção válida (1..5)\n");
                break;
        }
    }
    
    close(sockfd);
    return 0;
}


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

#define MARCADOR 0x7E
#define MAX_DADOS 127
#define TIMEOUT_MILLIS 1000
#define MAX_RETRANSMISSIONS 5


typedef struct {
    unsigned char marcador;
    unsigned char tamanho;
    unsigned char seq;
    unsigned char tipo;
    //unsigned int tamanho : 7;
    //unsigned int seq    : 5;
    //unsigned int tipo   : 4;
    unsigned char checksum;
    unsigned char dados[MAX_DADOS];
} __attribute__((packed)) Frame;

long long timestamp();
int cria_raw_socket(const char *interface);
unsigned char calcula_checksum(Frame *f);
void monta_frame(Frame *f, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam);
//void envia_mensagem(int sockfd, const char *interface, unsigned char seq, Frame f);
int espera_ack(int sockfd, unsigned char seq_esperado, int timeoutMillis);
int envia_mensagem(int sockfd, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam, int modo_servidor, struct sockaddr_ll* destino);
void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr);
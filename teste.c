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

typedef struct {
    unsigned char marcador;
    unsigned char tamanho;
    unsigned char seq_tipo;
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
    sum += f->seq_tipo;
    for (int i = 0; i < f->tamanho; ++i) {
        sum += f->dados[i];
    }
    return sum;
}

void envia_mensagem(int sockfd, const char *interface, unsigned char seq, unsigned char tipo) {
    Frame f;
    memset(&f, 0, sizeof(Frame));

    char mensagem[1024];
    printf("Digite uma mensagem: ");
    scanf("%s", mensagem);

    size_t len = strlen(mensagem);
    printf("asd\n");
    if (len > MAX_DADOS) len = MAX_DADOS;

    f.marcador = MARCADOR;
    f.tamanho = (unsigned char)len;
    f.seq_tipo = ((seq & 0x1F) << 3) | (tipo & 0x0F);
    memcpy(f.dados, mensagem, len);
    f.checksum = calcula_checksum(&f);

    // Monta destino (loopback: MAC dummy 00:00:00:00:00:00)
    struct sockaddr_ll dst = {0};
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFINDEX, &ifr);

    dst.sll_ifindex = ifr.ifr_ifindex;
    dst.sll_halen = 6;
    dst.sll_addr[0] = 0x00;

    int bytes = sendto(sockfd, &f, sizeof(Frame), 0, (struct sockaddr *)&dst, sizeof(dst));
    if (bytes < 0) perror("sendto");
    else printf("[Cliente] Enviado: '%s' (%d bytes)\n", mensagem, bytes);
}

void escuta_mensagem(int sockfd) {
    unsigned char buffer[2048];
    while (1) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes < 5) continue;

        for (ssize_t i = 0; i < bytes - 4; ++i) {
            if (buffer[i] == MARCADOR) {
                Frame *f = (Frame *)&buffer[i];
                unsigned char expected_checksum = calcula_checksum(f);
                if (f->checksum != expected_checksum) continue;

                unsigned char seq = (f->seq_tipo >> 3) & 0x1F;
                unsigned char tipo = f->seq_tipo & 0x0F;
                printf("[Servidor] Recebido seq=%u tipo=%u (%d bytes): '", seq, tipo, f->tamanho);
                for (int j = 0; j < f->tamanho; ++j) {
                    putchar(f->dados[j]);
                }
                printf("'\n");
                //return; // sai após 1 leitura
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <interface> <cliente|servidor>\n", argv[0]);
        return 1;
    }

    const char *interface = argv[1];
    int sockfd = cria_raw_socket(interface);

    if (strcmp(argv[2], "cliente") == 0) {
        envia_mensagem(sockfd, interface, 2, 3);
        envia_mensagem(sockfd, interface, 2, 3);
    } else {
        escuta_mensagem(sockfd);
    }

    close(sockfd);
    return 0;
}


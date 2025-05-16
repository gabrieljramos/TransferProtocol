#include "common.h"

long long timestamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000LL + tp.tv_usec / 1000;
}

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

int espera_ack(int sockfd, unsigned char seq_esperado, int timeoutMillis) {
   
    Frame resposta;

    //Define timeout
    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    long long inicio = timestamp();

    while (timestamp() - inicio <= timeoutMillis) {
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
            } else if ((resposta.tipo >= 6 && resposta.tipo <= 8) && resposta.seq == seq_esperado) { // OK + ACK
                printf("[Cliente] Arquivo recebido para seq=%u\n", seq_esperado);
                printf("File: %s\n", resposta.dados);

                // Envia ACK de volta para o servidor
                Frame ack_frame;
                monta_frame(&ack_frame, resposta.seq, 0, NULL, 0); // tipo 0 = ACK
                send(sockfd, &ack_frame, sizeof(Frame), 0);
                printf("[Cliente] ACK enviado para tipo=6,7,8 seq=%u\n", resposta.seq);

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
void envia_mensagem(int sockfd, const char *interface, unsigned char seq, Frame f) {

    int timeout = TIMEOUT_MILLIS;
    int tentativas = 0;
    int ack = 0;

    while (tentativas < MAX_RETRANSMISSIONS && !ack) {

        int bytes = send(sockfd, &f, sizeof(Frame), 0);
        if (bytes < 0) 
            perror("sendto");
        else 
            printf("[Cliente] Frame enviado (seq=%u)\n", seq);

        int resultado = espera_ack(sockfd, seq, timeout);

        if (resultado == 1) //Caso o ack tenha chego com sucesso
            ack = 1;
        else if (resultado == 2) { //Caso ACK tenha chego com sucesso e encontrado um tesouro
            ack = 1;
            resultado = espera_ack(sockfd, seq, timeout);
        }
        else { //timeout/NACK
            tentativas++;
            timeout *= 2;
        }
    }
    if (!ack) { //Caso num de tentativa tenha excedido o maximo
        printf("[Cliente] Falha após %d tentativas\n", MAX_RETRANSMISSIONS);
    }
}

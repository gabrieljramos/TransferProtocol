#include "common.h"
unsigned char *extensoes[] = {".txt", ".mp4", ".jpeg"};
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

void envia_resposta(int sockfd, unsigned char seq, unsigned char tipo, struct sockaddr_ll* origem, unsigned char *msg) {

    Frame resposta;

    if (msg == NULL)
        monta_frame(&resposta, seq, tipo, NULL, 0);
    else
        monta_frame(&resposta, seq, tipo, msg, strlen((char*)msg));

    send(sockfd, &resposta, sizeof(resposta), 0);
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

/*void cliente_recebe_arquivos(int sockfd) {
    while (1) {
        Frame resposta;
        ssize_t n = recv(sockfd, &resposta, sizeof(resposta), 0);

        if (n <= 0) break;

        if (resposta.marcador != MARCADOR) continue;

        unsigned char checksum_esperado = calcula_checksum(&resposta);
        if (resposta.checksum != checksum_esperado) {
            printf("[Cliente] Checksum inválido (esperado=%u, recebido=%u)\n", checksum_esperado, resposta.checksum);
            
            // Envia NACK
            Frame nack;
            monta_frame(&nack, resposta.seq, 1, NULL, 0); // tipo 1 = NACK
            send(sockfd, &nack, sizeof(Frame), 0);
            printf("[Cliente] NACK enviado para seq=%u\n", resposta.seq);
            continue;
        }

        if (resposta.tipo >= 6 && resposta.tipo <= 8) {
            printf("[Cliente] Arquivo recebido: %s\n", resposta.dados);

            // Envia ACK de volta
            Frame ack;
            monta_frame(&ack, resposta.seq, 0, NULL, 0); // tipo 0 = ACK
            send(sockfd, &ack, sizeof(Frame), 0);
            printf("[Cliente] ACK enviado para tipo=%u seq=%u\n", resposta.tipo, resposta.seq);
            break;
        } 
        else if (resposta.tipo == 15) { // sinal de fim de transmissão, opcional
            printf("[Cliente] Fim da transmissão de arquivos\n");
            break;
        }
        else {
            printf("[Cliente] Tipo inesperado recebido (tipo=%u)\n", resposta.tipo);
            break;
        }
    }
}*/

void envia_mensagem(int sockfd, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam, int modo_servidor, struct sockaddr_ll* destino) {
    int timeout = TIMEOUT_MILLIS;
    int tentativas = 0;
    int ack = 0;

    Frame f;
    monta_frame(&f, seq, tipo, dados, tam);

    while (tentativas < MAX_RETRANSMISSIONS && !ack) {
        int bytes;
        if (modo_servidor && destino != NULL) {
            bytes = sendto(sockfd, &f, sizeof(Frame), 0, (struct sockaddr*)destino, sizeof(struct sockaddr_ll));
        } else {
            bytes = send(sockfd, &f, sizeof(Frame), 0);
        }

        if (bytes < 0) {
            perror("send/sendto");
        } else {
            printf("[%s] Frame enviado (seq=%u, tipo=%u)\n", modo_servidor ? "Servidor" : "Cliente", seq, tipo);
        }

        int resultado = espera_ack(sockfd, seq, timeout);

        if (resultado == 1) {
            ack = 1;
        } 
        else if (resultado == 2 && !modo_servidor) {
            ack = 1;
            // Cliente recebeu OK+ACK ou dados de arquivo, entra no modo passivo
            escuta_mensagem(sockfd, 0, NULL, NULL, NULL);
        } 
        else {
            tentativas++;
            timeout *= 2;
        }
    }

    if (!ack) {
        printf("[%s] Falha após %d tentativas no seq=%u\n", modo_servidor ? "Servidor" : "Cliente", MAX_RETRANSMISSIONS, seq);
    }
}

void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr) {
    unsigned char buffer[2048];

    while (1) {
        struct sockaddr_ll addr;
        socklen_t addrlen = sizeof(addr);
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
        if (bytes < 5) continue;

        for (ssize_t i = 0; i < bytes - 4;) {
            if (buffer[i] == MARCADOR) {
                Frame *f = (Frame *)&buffer[i];
                unsigned char esperado = calcula_checksum(f);

                if (f->checksum != esperado) {
                    printf("[%s] Checksum inválido seq=%u\n", modo_servidor ? "Servidor" : "Cliente", f->seq);

                    Frame nack;
                    monta_frame(&nack, f->seq, 1, NULL, 0); // tipo 1 = NACK
                    send(sockfd, &nack, sizeof(Frame), 0);
                    i += sizeof(Frame);
                    continue;
                }

                unsigned char tipo = f->tipo & 0x0F;

                if (modo_servidor) {
                    // Ignora frames de controle
                    if (tipo == 0 || tipo == 1 || tipo == 2) {
                        i += sizeof(Frame);
                        continue;
                    }

                    // Processa movimento
                    if (tipo == 11) update_x('-', current_pos);
                    else if (tipo == 12) update_x('+', current_pos);
                    else if (tipo == 13) update_y('-', current_pos);
                    else if (tipo == 10) update_y('+', current_pos);

                    move_t *mv = malloc(sizeof(move_t));
                    mv->pos = *current_pos;
                    mv->next = mv->prev = NULL;
                    add_move(mv);
                    printf("[Servidor] Posição (%d, %d) salva\n", mv->pos.x, mv->pos.y);

                    int tesouro = treasure_found(tesouros, current_pos->x, current_pos->y);
                    if (tesouro >= 0) {
                        envia_resposta(sockfd, f->seq, 2, &addr, NULL); // OK+ACK
                        unsigned char nome[64];
                        int tipo_arq = find_file(tesouros[tesouro].id, nome);
                        if (tipo_arq >= 0) {
                            envia_mensagem(sockfd, f->seq, tipo_arq + 6, nome, strlen((char*)nome), 1, &addr);
                        }
                        else
                            return;
                    } else {
                        envia_resposta(sockfd, f->seq, 0, &addr, NULL); // ACK
                    }

                } else {
                    // Modo cliente, só aceita tipos 6–8
                    if (tipo >= 6 && tipo <= 8) {
                        printf("[Cliente] Arquivo recebido: %s\n", f->dados);
                        Frame ack;
                        monta_frame(&ack, f->seq, 0, NULL, 0); // ACK
                        send(sockfd, &ack, sizeof(Frame), 0);
                        return;
                    } else if (tipo == 15) {
                        printf("[Cliente] Fim da transmissão\n");
                        return;
                    } else {
                        printf("[Cliente] Tipo inesperado %u\n", tipo);
                    }
                }

                i += sizeof(Frame);
            } else {
                i++;
            }
        }
    }
}

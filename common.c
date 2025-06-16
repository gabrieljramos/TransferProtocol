#include "common.h"

unsigned char *extensoes[] = {".txt", ".mp4", ".jpeg"};

int steps_taken = 0;

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
        f->dados[tam] = '\0';
    }
    f->checksum = calcula_checksum(f);
}

int find_file(int file_num, unsigned char *file_name, size_t name_size, unsigned char *file_path, size_t path_size) {

    for (int i = 0; i < 3; i++) {
        snprintf(file_name, name_size, "%d", file_num);                                                 // Converte o número do arquivo para string
        snprintf(file_path, path_size, "./objetos/%s%s", file_name, extensoes[i]);                      // Concatena o caminho do arquivo com a extensão correspondente
        FILE *fp = fopen(file_path, "rb");

        if (fp) {
            printf("Arquivo encontrado: %s Tamanho(nome): %ld\n", file_name, strlen(file_name));
            fclose(fp);
            return i;                                                                                   // Retorna o índice da extensão encontrada                                      
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
            } else if (resposta.tipo == 15 && resposta.seq == seq_esperado) { // ERRO
                if (strcmp((char*)resposta.dados, "1") == 0) {              // Erro de espaço insuficiente
                    printf("[Servidor] Espaco insuficiente para o arquivo\n");
                    return 3;
                }
                else if (strcmp((char*)resposta.dados, "3") == 0) {         // Erro de jogo já iniciado
                    printf("[Servidor] Jogo ja iniciado, reiniciando...\n");
                    return 4;
                }
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

int envia_mensagem(int sockfd, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam, int modo_servidor, struct sockaddr_ll* destino) {
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
            return 1;
        } 
        else if (resultado == 2 && !modo_servidor) {
            ack = 1;
            escuta_mensagem(sockfd, 0, NULL, NULL, NULL); // Escuta a mensagem de tesouro recebida
        } else if(resultado == 3) {
            return -1;
        }
        else if (resultado == 4) {
            return 5;
        }
        else {
            tentativas++;
            timeout *= 2;
        }
    }

    if (!ack) {
        printf("[%s] Falha após %d tentativas no seq=%u\n", modo_servidor ? "Servidor" : "Cliente", MAX_RETRANSMISSIONS, seq);
    }

    return 0;

}

void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr) {
    unsigned char buffer[2048];

    while (1) {
        struct sockaddr_ll addr;
        socklen_t addrlen = sizeof(addr);
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
        if (bytes < 5) continue;                                                            // Verifica se o tamanho mínimo do frame é atendido

        for (ssize_t i = 0; i < bytes - 4;) {                                               // Percorre o buffer procurando por frames válidos
            if (buffer[i] == MARCADOR) {                                                    // Verifica se o marcador é válido
                Frame *f = (Frame *)&buffer[i];
                unsigned char esperado = calcula_checksum(f);

                if (f->checksum != esperado) {                                              // Verifica se o checksum é válido
                    printf("[%s] Checksum inválido seq=%u\n", modo_servidor ? "Servidor" : "Cliente", f->seq);

                    Frame nack;
                    monta_frame(&nack, f->seq, 1, NULL, 0);                                 // tipo 1 = NACK
                    send(sockfd, &nack, sizeof(Frame), 0);
                    i += sizeof(Frame);
                    continue;
                }

                unsigned char tipo = f->tipo & 0x0F;                                        // Extrai o tipo do frame

                if (modo_servidor) {
                    if (tipo == 0 || tipo == 1 || tipo == 2) {                              // Ignora ACK, NACK ou OK+ACK
                        i += sizeof(Frame);
                        continue;
                    }

                    if (tipo == 3) {                                                       // Tipo 3 = Jogo encerrado
                        printf("[Servidor] Jogo encerrado\n");
                        envia_resposta(sockfd, f->seq, 0, &addr, NULL);
                        return;
                    }

                    if (tipo == 14) {                                                       // Tipo 14 = Jogo em andamento
                        if (steps_taken > 0) {                                              // Se já houver passos dados, não permite iniciar o jogo
                            printf("[Servidor] Jogo ja iniciado, reiniciando...\n");
                            unsigned char error_code[2] = {'3', '\0'};
                            envia_resposta(sockfd, f->seq, 15, &addr, error_code);          // ERRO TIPO 3
                            return;
                        }
                        else {                                                              // Se não houver passos dados, inicia o jogo
                            printf("[Servidor] Jogo iniciado\n");
                            envia_resposta(sockfd, f->seq, 0, &addr, NULL); // ACK
                        }
                    }

                    // Processa movimento
                    if (tipo == 11) update_x('-', current_pos);
                    else if (tipo == 12) update_x('+', current_pos);
                    else if (tipo == 13) update_y('-', current_pos);
                    else if (tipo == 10) update_y('+', current_pos);

                    move_t *mv = malloc(sizeof(move_t));                                    // Aloca memória para o movimento
                    mv->pos = *current_pos;                         
                    mv->next = mv->prev = NULL;
                    add_move(mv);                                                           // Adiciona o movimento à lista de movimentos
                    printf("[Servidor] Posição (%d, %d) salva\n", mv->pos.x, mv->pos.y);
                    steps_taken++;

                    int tesouro = treasure_found(tesouros, current_pos->x, current_pos->y); // Verifica se um tesouro foi encontrado
                    if (tesouro >= 0) {
                        envia_resposta(sockfd, f->seq, 2, &addr, NULL);                     // OK+ACK
                        unsigned char nome[64];
                        unsigned char file_path[90];
                        int tipo_arq = find_file(tesouros[tesouro].id, nome, sizeof(nome), file_path, sizeof(file_path));
                        if (tipo_arq >= 0) {                                                                                    // Se o arquivo foi encontrado
                            struct stat filestat;
                            if (stat((char*)file_path, &filestat) != 0) {                                                       // Obtém informações do arquivo
                                perror("[Servidor] Erro ao obter informações do arquivo");
                                return;
                            }

                            unsigned char tamanho_str[16];
                            printf("[Servidor] st_size lido de stat(): %ld\n", (long)filestat.st_size);
                            snprintf(tamanho_str, sizeof(tamanho_str), "%ld", (long)filestat.st_size);

                            int authorized = envia_mensagem(sockfd, f->seq, 4, (unsigned char*)tamanho_str, strlen(tamanho_str), 1, &addr);

                            if (authorized) {                                               // Se o cliente autorizou o recebimento do arquivo       
                                printf("[Servidor] Enviando nome arquivo %s\n", nome);
                                envia_mensagem(sockfd, f->seq, tipo_arq + 6, nome, strlen((char*)nome), 1, &addr);
                            }

                        }
                        else {                                                              // Se o arquivo não foi encontrado
                            printf("[Servidor] Arquivo não encontrado para o tesouro %d\n", tesouros[tesouro].id);
                            unsigned char error_code[2] = {'2', '\0'};
                            envia_resposta(sockfd, f->seq, 15, &addr, error_code);
                        }
                    } else {                                                                // Se não foi encontrado nenhum tesouro
                        envia_resposta(sockfd, f->seq, 0, &addr, NULL); // ACK
                    }

                } else {
                    // Modo cliente, só aceita tipos 6–8
                    if (tipo >= 6 && tipo <= 8) {
                        printf("[Cliente] Arquivo recebido: %s\n", f->dados);
                        unsigned char file_name[140];
                        snprintf(file_name, sizeof(file_name), "%s%s", f->dados, extensoes[tipo - 6]); // Concatena o nome do arquivo com a extensão
                        printf("[Cliente] Nome do arquivo: %s\n", file_name);
                        envia_resposta(sockfd, f->seq, 0, cliente_addr, NULL); // ACK
                        return;
                    } else if (tipo == 4) {
                        printf("[Cliente] Tamanho do arquivo recebido: %s bytes\n", f->dados);
                        long file_size = strtol((char*)f->dados, NULL, 10);
                        printf("[Cliente] Tamanho do arquivo (long): %ld bytes\n", file_size);
                        struct statvfs statv;
                        if (statvfs(".", &statv) != 0) {
                            perror("statvfs");
                            return;
                        }

                        long long livre = (long long)statv.f_bsize * statv.f_bavail;
                        long long margem = file_size + (file_size / 10); // 10% de margem

                        if (livre >= margem) {
                            printf("[Cliente] Espaço suficiente (%lld bytes disponíveis)\n", livre);
                            envia_resposta(sockfd, f->seq, 0, cliente_addr, NULL); // ACK
                        } else {
                            printf("[Cliente] Espaço insuficiente (%lld bytes livres)\n", livre);
                            unsigned char error_code[2] = {'1', '\0'};
                            envia_resposta(sockfd, f->seq, 15, cliente_addr, error_code); // tipo 15 = erro
                        }
                        //return;
                    } else if (tipo == 5) {
                        printf("[Cliente] Arquivo recebido: %s\n", f->dados);
                        Frame ack;
                        monta_frame(&ack, f->seq, 0, NULL, 0); // ACK
                        send(sockfd, &ack, sizeof(Frame), 0);
                        return;
                    } else if (tipo == 14) {
                        printf("Arquivo não encontrado\n");
                        return;
                    }
                    else if (tipo == 15) {
                        if (strcmp((char*)f->dados, "2") == 0)
                            printf("File Not Found, Error Code: %s\n", f->dados);
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
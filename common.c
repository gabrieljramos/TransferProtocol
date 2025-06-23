#include "common.h"

#define FRAME_HEADER_SIZE (sizeof(Frame) - 127)
#define MIN_ETH_PAYLOAD_SIZE 14

unsigned char *extensoes[] = {".txt", ".mp4", ".jpg"};

int steps_taken = 0;
int treasures_found = 0;
int last_seq = -1;

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

    size_t bytes_to_send = FRAME_HEADER_SIZE + resposta.tamanho;
    int bytes_sent;

    if (bytes_to_send < MIN_ETH_PAYLOAD_SIZE) {   // Se o tamanho do frame for menor que o mínimo, preenche com zeros

        unsigned char padded_buffer[MIN_ETH_PAYLOAD_SIZE] = {0};
        memcpy(padded_buffer, &resposta, bytes_to_send);

        if (origem != NULL) {
            bytes_sent = sendto(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0, (struct sockaddr*)origem, sizeof(struct sockaddr_ll));
        } else {
            bytes_sent = send(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0);
        }
    } else {            // Se o tamanho do frame for maior ou igual ao mínimo, envia normalmente
        if (origem != NULL) {
            bytes_sent = sendto(sockfd, &resposta, bytes_to_send, 0, (struct sockaddr*)origem, sizeof(struct sockaddr_ll));
        } else {
            bytes_sent = send(sockfd, &resposta, bytes_to_send, 0);
        }
    }

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

int servidor_envia_arquivo(int sockfd, unsigned char seq, const char* file_path, struct sockaddr_ll* destino) {

    FILE *fp = fopen(file_path, "rb");                                                                  // Abre o arquivo a ser enviado
    if (!fp) {
        perror("[Servidor] Falha ao abrir arquivo de envio\n");
        return -1;
    }

    printf("[Servidor] Iniciando envio de: %s\n", file_path);
    unsigned char chunk_buffer[127];
    size_t bytes_read;

    while ((bytes_read = fread(chunk_buffer, 1, sizeof(chunk_buffer), fp)) > 0) {                       // Lê o arquivo em pedaços de 127 bytes
  
        if (!envia_mensagem(sockfd, seq, 5, chunk_buffer, bytes_read, 1, destino)) {                    // Envia o pedaço do arquivo
            printf("[Servidor] Falha ao receber ACK para o pedaço do arquivo seq=%u. Abortando.\n", seq);
            fclose(fp);
            return -1;
        }
        seq = (seq + 1) % 32;                                                                           // Incrementa o número de sequência para o próximo pedaço
    }

    printf("[Servidor] Enviando EOF.\n");
    envia_mensagem(sockfd, seq, 9, NULL, 0, 1, destino);
    
    seq = (seq + 1) % 32;                                                                               // Incrementa o número de sequência para o EOF
    fclose(fp);
    return seq;                                                                                         // Retorna o próximo número de sequência disponível

}

int cliente_recebe_arquivo(int sockfd, const char* file_name, unsigned char seq) {

    FILE *fp = fopen(file_name, "wb");
    if (!fp) {
        perror("[Cliente] Falha ao criar arquivo para recebimento");
        return 0;
    }

    printf("[Cliente] Pronto para receber dados do arquivo: %s. Esperando seq=%u\n", file_name, seq);
    Frame f;
    int success = 0; 

    struct timeval sock_timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&sock_timeout, sizeof(sock_timeout));

    while (1) {
        long long inicio = timestamp();
        int chunk_received = 0;


        while (timestamp() - inicio <= TIMEOUT_MILLIS) { 
            ssize_t n = recv(sockfd, &f, sizeof(f), 0);

            if (n < 0) { 
                continue;
            }

            if (n < 5 || f.marcador != MARCADOR) {
                continue;
            }
            
            if (f.seq == seq) {
                if (f.tipo == 5) { // Data chunk
                    fwrite(f.dados, 1, f.tamanho, fp);
                    envia_resposta(sockfd, seq, 0, NULL, NULL);
                    printf("[Cliente] Bloco recebido seq=%u, tam=%u. ACK Enviado.\n", seq, f.tamanho);
                    seq = (seq + 1) % 32;
                    chunk_received = 1;
                    break; 
                } else if (f.tipo == 9) { 
                    envia_resposta(sockfd, seq, 0, NULL, NULL);
                    printf("[Cliente] Recebido marcador de Fim de Arquivo seq=%u. Transferencia completa.\n", seq);
                    chunk_received = 1;
                    success = 1; 
                    break;
                }
            }
        } 

        if (!chunk_received) {
            printf("[Cliente] Timeout esperando pelo bloco de arquivo com seq=%u. Abortando.\n", seq);
            success = 0;
            break;
        }

        if (success) {
            break;
        }
    }

    fclose(fp);

    if (!success) {
        remove(file_name);
    }

    sock_timeout.tv_sec = 0;
    sock_timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&sock_timeout, sizeof(sock_timeout));
    
    return success;
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
            printf("[Cliente/Servidor] Recebido %zd bytes\n", n);
            printf("[Cliente/Servidor] Dados recebidos: ");
            for (int i = 0; i < n; i++) printf("%02X ", ((unsigned char*)&resposta)[i]);
            printf("\n");

            if (resposta.tipo == 0 && resposta.seq == seq_esperado) { //ACK
                printf("[Cliente/Servidor] ACK recebido para seq=%u\n", seq_esperado);
                return 1;
            } else if (resposta.tipo == 1 && resposta.seq == seq_esperado) { //NACK
                printf("[Cliente/Servidor] NACK recebido para seq=%u\n", seq_esperado);
                return -1;
            } else if (resposta.tipo == 2 && resposta.seq == seq_esperado) { // OK + ACK
                printf("[Cliente] Tesouro recebido para seq=%u\n", seq_esperado);
                return 2;
            } else if (resposta.tipo == 15 && resposta.seq == seq_esperado) { // ERRO
                if (strcmp((char*)resposta.dados, "0") == 0) {              // Erro de permissao
                    printf("[Cliente] Sem permissao de leitura\n");
                    return 3;
                }
                else if (strcmp((char*)resposta.dados, "1") == 0) {              // Erro de espaço insuficiente
                    printf("[Servidor] Espaco insuficiente para o arquivo\n");
                    return 4;
                }
                else if (strcmp((char*)resposta.dados, "3") == 0) {         // Erro de jogo já iniciado
                    printf("[Cliente] Jogo ja iniciado, reiniciando...\n");
                    return 5;
                }
            }
            else {
                printf("[Cliente/Servidor] Frame ignorado (tipo=%u, seq=%u)\n", resposta.tipo, resposta.seq);
                continue;
            }
        } else {
            printf("[Cliente/Servidor] Timeout esperando ACK/NACK para seq=%u\n", seq_esperado);
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
        size_t bytes_to_send = FRAME_HEADER_SIZE + f.tamanho;

        if (bytes_to_send < MIN_ETH_PAYLOAD_SIZE) {         // Se o tamanho do frame for menor que o mínimo, preenche com zeros
            unsigned char padded_buffer[MIN_ETH_PAYLOAD_SIZE] = {0};
            memcpy(padded_buffer, &f, bytes_to_send);
            bytes = sendto(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0, (struct sockaddr*)destino, sizeof(struct sockaddr_ll));
        } else {                                            // Se o tamanho do frame for maior ou igual ao mínimo, envia normalmente
            bytes = sendto(sockfd, &f, bytes_to_send, 0, (struct sockaddr*)destino, sizeof(struct sockaddr_ll));
        }

        if (bytes < 0) {
            perror("send/sendto");
        } else {
            printf("[%s] Frame enviado (seq=%u, tipo=%u)\n", modo_servidor ? "Servidor" : "Cliente", seq, tipo);
        }

        int resultado = espera_ack(sockfd, seq, timeout);

        if (resultado == 1) { // ACK recebido
            return 1;
        } else if (resultado == -1) { // NACK recebido
            printf("[%s] NACK recebido para seq=%u, retransmitindo...\n", modo_servidor ? "Servidor" : "Cliente", seq);
            tentativas++;
        }
        else if (resultado == 2) {  // Tesouro recebido
            escuta_mensagem(sockfd, 0, NULL, NULL, NULL); // Escuta a mensagem de tesouro recebida
            return 2;
        } else if(resultado == 4) { // Erro de espaço insuficiente
            return -1;
        }
        else if (resultado == 5) { // Erro de jogo já iniciado
            return 5;
        }
        else if (resultado == 0) { // Timeout ou erro ao receber ACK/NACK
            tentativas++;
            timeout *= 2;
        }
    }

    if (!ack) {
        printf("[%s] Falha após %d tentativas no seq=%u\n", modo_servidor ? "Servidor" : "Cliente", MAX_RETRANSMISSIONS, seq);
    }

    return 0; // Falha ao enviar a mensagem após o número máximo de tentativas

}

void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr) {
    unsigned char buffer[2048];

    while (1) {
        struct sockaddr_ll addr;
        socklen_t addrlen = sizeof(addr);
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
        if (bytes < FRAME_HEADER_SIZE + 1) {
            continue;
        }

        for (ssize_t i = 0; i < bytes; ) {
            if (buffer[i] != MARCADOR) {
                i++;
                continue;
            }
            if (bytes - i < FRAME_HEADER_SIZE) {
                break;
            }

            unsigned char *ptr = &buffer[i];
            unsigned char tipo = (ptr[1] >> 4) & 0x0F;
            unsigned char seq = (unsigned char)(((ptr[1] & 0x0F) << 1) | ((ptr[2] >> 7) & 0x01));
            unsigned char tamanho = ptr[2] & 0x7F;
            unsigned char checksum = ptr[3];
            unsigned char *dados_ptr = &ptr[4];

            if (bytes - i < FRAME_HEADER_SIZE + tamanho) {
                // Frame incompleto no buffer atual
                break;
            }

            // Cálculo manual de checksum: byte1, byte2 e dados
            unsigned char checksum_calc = 0;
            checksum_calc += ptr[1];
            checksum_calc += ptr[2];
            for (int j = 0; j < tamanho; ++j) {
                checksum_calc += ptr[4 + j];
            }
            if (checksum != checksum_calc) {
                printf("[%s] Checksum inválido (manual) seq=%u: recebido=0x%02x, calc=0x%02x\n",
                       modo_servidor ? "Servidor" : "Cliente", seq, checksum, checksum_calc);
                envia_resposta(sockfd, seq, 1, &addr, NULL); // NACK
                i++;
                continue;
            }

            int is_duplicate = 0;
            if (tipo >= 10 && tipo <= 13 && seq == last_seq) {
                is_duplicate = 1;
                if (modo_servidor) {
                    printf("[Servidor] Duplicado de movimento recebido (seq=%u).\n", seq);
                } else {
                    printf("[Cliente] Duplicado de movimento recebido (seq=%u).\n", seq);
                }
            }

            if (modo_servidor) {
                // IGNORA ACK/NACK/OK+ACK: tipos 0,1,2
                if (tipo == 0 || tipo == 1 || tipo == 2) {
                    i += FRAME_HEADER_SIZE + tamanho;
                    continue;
                }
                if (tipo == 3) {
                    printf("[Servidor] Jogo encerrado\n");
                    envia_resposta(sockfd, seq, 0, &addr, NULL);
                    return;
                }
                if (tipo == 14) {
                    if (steps_taken > 0) {
                        printf("[Servidor] Jogo já iniciado, reiniciando...\n");
                        unsigned char error_code[2] = {'3', '\0'};
                        envia_resposta(sockfd, seq, 15, &addr, error_code);
                        return;
                    } else {
                        printf("[Servidor] Jogo iniciado\n");
                        envia_resposta(sockfd, seq, 0, &addr, NULL);
                    }
                }
                if (!is_duplicate) {
                    if (tipo == 11) update_x('-', current_pos);
                    else if (tipo == 12) update_x('+', current_pos);
                    else if (tipo == 13) update_y('-', current_pos);
                    else if (tipo == 10) update_y('+', current_pos);
                    add_move(current_pos);
                    steps_taken++;
                    last_seq = seq;
                }
                int tesouro = treasure_found(tesouros, current_pos->x, current_pos->y);
                if (tesouro >= 0) {
                    envia_resposta(sockfd, seq, 2, &addr, NULL);
                    if (!is_duplicate) {
                        tesouros[tesouro].encontrado = 1;
                        print_info(tesouros);
                        treasures_found++;
                    }
                    unsigned char nome[64];
                    unsigned char file_path[90];
                    int tipo_arq = find_file(tesouros[tesouro].id, nome, sizeof(nome), file_path, sizeof(file_path));
                    if (tipo_arq >= 0) {
                        if (access((char*)file_path, R_OK) == -1) {
                            perror("[Servidor] Erro de permissão de acesso ao arquivo");
                            unsigned char error_code[2] = {'0', '\0'};
                            envia_resposta(sockfd, seq, 15, &addr, error_code);
                            i += FRAME_HEADER_SIZE + tamanho;
                            continue;
                        }
                        printf("[Servidor] Arquivo encontrado para o tesouro %d: %s\n", tesouros[tesouro].id, nome);
                        struct stat filestat;
                        if (stat((char*)file_path, &filestat) != 0) {
                            perror("[Servidor] Erro ao obter informações do arquivo");
                            return;
                        }
                        unsigned char tamanho_str[16];
                        printf("[Servidor] st_size lido de stat(): %ld\n", (long)filestat.st_size);
                        snprintf((char*)tamanho_str, sizeof(tamanho_str), "%ld", (long)filestat.st_size);

                        int authorized = envia_mensagem(sockfd, seq, 4, tamanho_str, strlen((char*)tamanho_str), 1, &addr);
                        if (authorized == 1) {
                            unsigned char next_seq = (seq + 1) % 32;
                            printf("[Servidor] Enviando arquivo %s com extensão %s\n", nome, extensoes[tipo_arq]);
                            int name_sent = envia_mensagem(sockfd, next_seq, tipo_arq + 6, nome, strlen((char*)nome), 1, &addr);
                            if (name_sent == 1) {
                                next_seq = (next_seq + 1) % 32;
                                servidor_envia_arquivo(sockfd, next_seq, (char*)file_path, &addr);
                            } else {
                                printf("[Servidor] Falha ao enviar nome do arquivo %s\n", nome);
                                return;
                            }
                        } else if (authorized == 4) {
                            printf("[Servidor] Cliente não tem espaço suficiente para o arquivo\n");
                            i += FRAME_HEADER_SIZE + tamanho;
                            continue;
                        } else if (authorized == 0) {
                            printf("[Servidor] Resposta nao recebida\n");
                            return;
                        }
                        if (treasures_found >= 8) {
                            printf("[Servidor] Todos os tesouros encontrados! Jogo encerrado.\n");
                            return;
                        }
                    } else {
                        printf("[Servidor] Arquivo não encontrado para o tesouro %d\n", tesouros[tesouro].id);
                        unsigned char error_code[2] = {'2', '\0'};
                        envia_resposta(sockfd, seq, 15, &addr, error_code);
                    }
                } else {
                    print_info();
                    envia_resposta(sockfd, seq, 0, &addr, NULL);
                }
            } else {
                // Modo cliente
                if (tipo >= 6 && tipo <= 8) {
                    unsigned char file_name[140];
                    // dados_ptr aponta para f.dados contido no buffer
                    snprintf((char*)file_name, sizeof(file_name), "%s%s", (char*)dados_ptr, (char*)extensoes[tipo - 6]);
                    printf("[Cliente] Nome do arquivo recebido: %s\n", file_name);
                    envia_resposta(sockfd, seq, 0, NULL, NULL);
                    unsigned char next_seq = (seq + 1) % 32;
                    if (cliente_recebe_arquivo(sockfd, (char*)file_name, next_seq)) {
                        return;
                    }
                } else if (tipo == 4) {
                    printf("[Cliente] Tamanho do arquivo recebido: %s bytes\n", dados_ptr);
                    long file_size = strtol((char*)dados_ptr, NULL, 10);
                    printf("[Cliente] Tamanho do arquivo (long): %ld bytes\n", file_size);
                    struct statvfs statv;
                    if (statvfs(".", &statv) != 0) {
                        perror("statvfs");
                        return;
                    }
                    long long livre = (long long)statv.f_bsize * statv.f_bavail;
                    long long margem = file_size + (file_size / 10);
                    if (livre >= margem) {
                        printf("[Cliente] Espaço suficiente (%lld bytes disponíveis)\n", livre);
                        envia_resposta(sockfd, seq, 0, cliente_addr, NULL);
                    } else {
                        printf("[Cliente] Espaço insuficiente (%lld bytes livres)\n", livre);
                        unsigned char error_code[2] = {'1', '\0'};
                        envia_resposta(sockfd, seq, 15, cliente_addr, error_code);
                        return;
                    }
                } else if (tipo == 15) {
                    if (dados_ptr && strcmp((char*)dados_ptr, "2") == 0) {
                        printf("[Cliente] Arquivo nao encontrado, Error Code: %s\n", dados_ptr);
                    } else if (dados_ptr && strcmp((char*)dados_ptr, "0") == 0) {
                        printf("[Cliente] Permissao negada, Error Code: %s\n", dados_ptr);
                    }
                    return;
                } else {
                    printf("[Cliente] Tipo inesperado %u\n", tipo);
                }
            }

            i += FRAME_HEADER_SIZE + tamanho;
        } // for i < bytes
    } // while
}


// A função monta_pacote_manual corrigida:
int monta_pacote_manual(unsigned char* buffer, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam) {
    // Assegura que tam caiba em 7 bits
    if (tam > 127) tam = 127;

    // Byte 0: MARCADOR
    buffer[0] = MARCADOR;

    // Byte 1: tipo (4 bits) e bits altos de seq (4 bits: seq >> 1)
    buffer[1] = (unsigned char)(((tipo & 0x0F) << 4) | ((seq >> 1) & 0x0F));
    // Byte 2: bit baixo de seq (1 bit em MSB) e tamanho (7 bits)
    buffer[2] = (unsigned char)(((seq & 0x01) << 7) | (tam & 0x7F));
    // Byte 3: placeholder checksum
    buffer[3] = 0;

    // Bytes de dados
    if (dados && tam > 0) {
        memcpy(&buffer[4], dados, tam);
    }

    // Cálculo de checksum sobre byte1, byte2 e dados
    unsigned char checksum = 0;
    checksum += buffer[1];
    checksum += buffer[2];
    for (size_t i = 0; i < tam; ++i) {
        checksum += buffer[4 + i];
    }
    buffer[3] = checksum;

    return (int)(FRAME_HEADER_SIZE + tam);
}
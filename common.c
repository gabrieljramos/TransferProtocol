#include "common.h"

#define FRAME_HEADER_SIZE (sizeof(Frame) - 127)
#define MIN_ETH_PAYLOAD_SIZE 14
#define VLAN_BYTE_1 0x81
#define VLAN_BYTE_2 0x88
#define ESCAPE_BYTE 0xFF

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

// ==============================================================================================================================

int insere_vlan(unsigned char *dados, int capacidade, int *tamanho_atual) {
    int contador = 0;
    // Iteramos de trás para frente
    for (int i = (*tamanho_atual) - 1; i >= 0; i--) {
        if (dados[i] == VLAN_BYTE_1 || dados[i] == VLAN_BYTE_2) {
            // Verifica se há espaço para inserir mais um byte.
            if (*tamanho_atual >= capacidade) {
                fprintf(stderr, "Erro: Buffer cheio. Impossível inserir byte de escape.\n");
                return -1; // Retorna erro
            }

            // Desloca todos os bytes da posição i+1 em diante para a direita.
            memmove(&dados[i + 2], &dados[i + 1], (*tamanho_atual) - (i + 1));

            // Insere o byte de escape.
            dados[i + 1] = ESCAPE_BYTE;

            // Atualiza o tamanho e o contador.
            (*tamanho_atual)++;
            contador++;
        }
    }
    return contador;
}

int remove_vlan(unsigned char *dados, int *tamanho_atual) {
    int write_idx = 0; // Ponteiro de escrita
    int read_idx = 0;  // Ponteiro de leitura
    int contador = 0;

    while (read_idx < *tamanho_atual) {
        // Copia o byte atual
        dados[write_idx] = dados[read_idx];

        // Verifica se o byte que acabamos de copiar é um dos problemáticos
        if ((dados[write_idx] == VLAN_BYTE_1 || dados[write_idx] == VLAN_BYTE_2) && (read_idx + 1 < *tamanho_atual) && (dados[read_idx + 1] == ESCAPE_BYTE)) {
            // Se for, e o próximo for um byte de escape, pulamos o byte de escape
            read_idx++; // Incrementa o ponteiro de leitura para pular o ESCAPE_BYTE
            contador++; // Conta como um byte removido
        }
        
        write_idx++;
        read_idx++;
    }

    // O novo tamanho dos dados é a posição final do ponteiro de escrita
    *tamanho_atual = write_idx;
    return contador;
}

// ==============================================================================================================================
// ==============================================================================================================================

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

unsigned char calcula_checksum_manual(const unsigned char *buffer, size_t tam) {
    unsigned char sum = 0;
    // Sum the two bytes containing tipo, seq, and tamanho
    sum += buffer[1];
    sum += buffer[2];
    // Sum the data payload, which starts at index 4
    for (size_t i = 0; i < tam; ++i) {
        sum += buffer[4 + i];
    }
    return sum;
}

// ==============================================================================================================================
// ==============================================================================================================================

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

// ==============================================================================================================================
// ==============================================================================================================================

unsigned char* monta_frame(unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam) {
    size_t data_size = tam;

    if (tipo == 0 || tipo == 1 || tipo == 2 || tipo == 3 || tipo == 14 || (tipo >= 10 && tipo <= 13)) {
        data_size = 0;
    } else {
        if (data_size > 127) {
            data_size = 127;
        }
    }


    unsigned char *frame_buffer = (unsigned char*) malloc(4 + data_size);
    if (!frame_buffer) {
        perror("Malloc monta_frame");
        return NULL;
    }
    frame_buffer[0] = MARCADOR;
    frame_buffer[1] = ((tipo & 0x0F) << 4) | ((seq >> 1) & 0x0F);
    frame_buffer[2] = ((seq & 0x01) << 7) | (data_size & 0x7F);

    frame_buffer[3] = 0;


    if (data_size > 0 && dados != NULL) {
        memcpy(&frame_buffer[4], dados, data_size);
    }

    frame_buffer[3] = calcula_checksum_manual(frame_buffer, data_size);

    return frame_buffer; 
}

// ==============================================================================================================================
// ==============================================================================================================================

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

/*int cliente_recebe_arquivo(int sockfd, const char* file_name, unsigned char seq, struct sockaddr_ll* server_addr) {

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

            unsigned char temp_buffer[2048];
            ssize_t n = recv(sockfd, temp_buffer, sizeof(temp_buffer), 0);
            
            if (n < 0) { 
                continue;
            }

            int tam_buffer = n;
            remove_vlan(temp_buffer, &tam_buffer);

            if (n < 0) { 
                continue;
            }

            if (n < 5 || f.marcador != MARCADOR) {
                continue;
            }

            if (tam_buffer >= 4 && temp_buffer[0] == MARCADOR) {
                // --- Manual Unpacking ---
                unsigned char tipo = (temp_buffer[1] >> 4) & 0x0F;
                unsigned char seq = ((temp_buffer[1] & 0x0F) << 1) | ((temp_buffer[2] >> 7) & 0x01);
                unsigned char tam = temp_buffer[2] & 0x7F;
                unsigned char checksum_recebido = temp_buffer[3];
                unsigned char* dados = &temp_buffer[4];

                Frame f;
                f.tipo = tipo;
                f.seq = seq;
                f.tamanho = tam;
                memcpy(f.dados, &temp_buffer[4], tam);

                // --- Verify Checksum ---
                unsigned char checksum_calculado = calcula_checksum_manual(temp_buffer, tam);
                if (checksum_recebido != checksum_calculado) {
                    printf("[Cliente] Checksum inválido no bloco de arquivo seq=%u.\n", seq);
                    continue;
                }
            
                if (f.seq == seq) {
                    if (f.tipo == 5) { // Data chunk
                        fwrite(f.dados, 1, f.tamanho, fp);
                        envia_resposta(sockfd, seq, 0, server_addr, NULL);
                        printf("[Cliente] Bloco recebido seq=%u, tam=%u. ACK Enviado.\n", seq, f.tamanho);
                        seq = (seq + 1) % 32;
                        chunk_received = 1;
                        break; 
                    } else if (f.tipo == 9) { 
                        envia_resposta(sockfd, seq, 0, server_addr, NULL);
                        printf("[Cliente] Recebido marcador de Fim de Arquivo seq=%u. Transferencia completa.\n", seq);
                        chunk_received = 1;
                        success = 1; 
                        break;
                    }
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
}*/

int cliente_recebe_arquivo(int sockfd, const char* file_name, unsigned char seq_esperado, struct sockaddr_ll* server_addr) {
    FILE *fp = fopen(file_name, "wb");
    if (!fp) {
        perror("[Cliente] Falha ao criar arquivo para recebimento");
        return 0;
    }

    printf("[Cliente] Pronto para receber dados do arquivo: %s. Esperando seq=%u\n", file_name, seq_esperado);

    struct timeval sock_timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&sock_timeout, sizeof(sock_timeout));

    int tentativas = 0;
    while (tentativas < MAX_RETRANSMISSIONS) {
        unsigned char temp_buffer[2048];
        ssize_t n = recv(sockfd, temp_buffer, sizeof(temp_buffer), 0);

        if (n < 0) {
            printf("[Cliente] Tentativa %d: Timeout no recv, esperando novamente.\n", tentativas + 1);
            tentativas++;
            continue;
        }

        int tam_buffer = n;
        remove_vlan(temp_buffer, &tam_buffer);
        
        int frame_processed = 0;
        for (ssize_t i = 0; i <= tam_buffer - 4; ++i) {
            if (temp_buffer[i] == MARCADOR) {
                unsigned char tipo = (temp_buffer[i+1] >> 4) & 0x0F;
                unsigned char seq = ((temp_buffer[i+1] & 0x0F) << 1) | ((temp_buffer[i+2] >> 7) & 0x01);
                unsigned char tam = temp_buffer[i+2] & 0x7F;
                unsigned char checksum_recebido = temp_buffer[i+3];
                unsigned char* dados = &temp_buffer[i+4];

                unsigned char checksum_calculado = calcula_checksum_manual(&temp_buffer[i], tam);
                if (checksum_recebido != checksum_calculado) {
                    continue;
                }

                if (seq == seq_esperado) {
                    if (tipo == 5) { // Dados do arq
                        fwrite(dados, 1, tam, fp);
                        envia_resposta(sockfd, seq, 0, server_addr, NULL);
                        printf("[Cliente] Bloco recebido seq=%u, tam=%u. ACK Enviado.\n", seq, tam);
                        seq_esperado = (seq_esperado + 1) % 32;
                        tentativas = 0; 
                        frame_processed = 1;
                    } else if (tipo == 9) { // EOF
                        envia_resposta(sockfd, seq, 0, server_addr, NULL);
                        printf("[Cliente] Recebido marcador de Fim de Arquivo seq=%u. Transferencia completa.\n", seq);
                        fclose(fp);
                        sock_timeout.tv_sec = 0; sock_timeout.tv_usec = 0;
                        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&sock_timeout, sizeof(sock_timeout));
                        return 1;
                    }
                }
                i += 4 + tam - 1;
            }
        }
        if (!frame_processed) {
            tentativas++;
        }
    }

    // If we exit the loop, we've failed
    printf("[Cliente] Falha ao receber bloco de arquivo após %d tentativas. Abortando.\n", MAX_RETRANSMISSIONS);
    fclose(fp);
    remove(file_name);
    sock_timeout.tv_sec = 0; sock_timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&sock_timeout, sizeof(sock_timeout));
    return 0; // FAILURE
}

// ==============================================================================================================================
// ==============================================================================================================================

void envia_resposta(int sockfd, unsigned char seq, unsigned char tipo, struct sockaddr_ll* origem, unsigned char *msg) {

    unsigned char* resposta;
    int tam;

    if (msg == NULL) {
        tam = 0;
        resposta = monta_frame(seq, tipo, NULL, tam);
    }
    else {
        tam = strlen((char*)msg);
        resposta = monta_frame(seq, tipo, msg, tam);
    }

    size_t data_size = tam;
    if (msg == NULL) {
        data_size = 0;
    }
    int tam_original = 4 + data_size;
    
    unsigned char send_buffer[2048];
    memcpy(send_buffer, resposta, tam_original); // Copia o frame original

    int tamanho_para_enviar = tam_original;
    insere_vlan(send_buffer, sizeof(send_buffer), &tamanho_para_enviar);

    size_t bytes_to_send = tamanho_para_enviar; // Atualiza o tamanho após a inserção do VLAN
    int bytes_sent;

    if (bytes_to_send < MIN_ETH_PAYLOAD_SIZE) {   // Se o tamanho do frame for menor que o mínimo, preenche com zeros

        unsigned char padded_buffer[MIN_ETH_PAYLOAD_SIZE] = {0};
        memcpy(padded_buffer, send_buffer, bytes_to_send);

        if (origem != NULL) {
            bytes_sent = sendto(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0, (struct sockaddr*)origem, sizeof(struct sockaddr_ll));
        } else {
            bytes_sent = send(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0);
        }
    } else {            // Se o tamanho do frame for maior ou igual ao mínimo, envia normalmente
        if (origem != NULL) {
            bytes_sent = sendto(sockfd, send_buffer, bytes_to_send, 0, (struct sockaddr*)origem, sizeof(struct sockaddr_ll));
        } else {
            bytes_sent = send(sockfd, send_buffer, bytes_to_send, 0);
        }
    }
    free(resposta);

}

int espera_resposta(int sockfd, unsigned char seq_esperado, int timeoutMillis) {

    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    long long inicio = timestamp();

    while (timestamp() - inicio <= timeoutMillis) {
        unsigned char temp_buffer[2048];
        ssize_t n = recv(sockfd, temp_buffer, sizeof(temp_buffer), 0);
        
        if (n <= 0) {
            continue;
        }

        int tam_buffer = n;
        remove_vlan(temp_buffer, &tam_buffer);
        
        for (ssize_t i = 0; i <= tam_buffer - 4; ++i) {
            if (temp_buffer[i] == MARCADOR) {

                unsigned char tipo = (temp_buffer[i+1] >> 4) & 0x0F;
                unsigned char seq = ((temp_buffer[i+1] & 0x0F) << 1) | ((temp_buffer[i+2] >> 7) & 0x01);
                unsigned char tam = temp_buffer[i+2] & 0x7F;
                unsigned char checksum_recebido = temp_buffer[i+3];

                unsigned char checksum_calculado = calcula_checksum_manual(&temp_buffer[i], tam);

                if (checksum_recebido != checksum_calculado) {
                    continue; 
                }

                if (seq == seq_esperado) {
                    if (tipo == 0) return 1;    // ACK
                    if (tipo == 1) return -1;   // NACK
                    if (tipo == 2) return 2;    // OK + ACK (Treasure)
                    if (tipo == 15) {           // ERROR
                        if (strcmp((char*)&temp_buffer[i+4], "0") == 0) return 3; // Permissao
                        if (strcmp((char*)&temp_buffer[i+4], "1") == 0) return 4; // Espaço
                        if (strcmp((char*)&temp_buffer[i+4], "3") == 0) return 5; // Jogo iniciado
                    }
                }
                i += 4 + tam -1;
            }
        }
    }
    
    printf("[Cliente/Servidor] Timeout esperando ACK/NACK para seq=%u\n", seq_esperado);
    return 0;
}

// ==============================================================================================================================
// ==============================================================================================================================

int envia_mensagem(int sockfd, unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam, int modo_servidor, struct sockaddr_ll* destino) {

    int timeout = TIMEOUT_MILLIS;
    int tentativas = 0;
    int ack = 0;

    //Frame f;
    unsigned char *f = monta_frame(seq, tipo, dados, tam);

    size_t data_size = tam;
    // Recalculate the actual data size based on the type, just like in monta_frame
    if (tipo == 0 || tipo == 1 || tipo == 2 || tipo == 3 || tipo == 14 || (tipo >= 10 && tipo <= 13)) {
        data_size = 0;
    } else {
        if (data_size > 127) data_size = 127;
    }
    
    // The correct size is always the manual header size (4) plus the actual data_size
    int tam_original = 4 + data_size;
    printf("[Cliente DEBUG] Enviando %d bytes. Marcador no buffer: 0x%02X\n", tam_original, f[0]);

    while (tentativas < MAX_RETRANSMISSIONS && !ack) {
        int bytes;
        
        unsigned char send_buffer[2048];
        memcpy(send_buffer, f, tam_original);

        int tamanho_para_enviar = tam_original;
        insere_vlan(send_buffer, sizeof(send_buffer), &tamanho_para_enviar);
        int bytes_to_send = tamanho_para_enviar; // Atualiza o tamanho após a inserção do VLAN

        if (bytes_to_send < MIN_ETH_PAYLOAD_SIZE) {         // Se o tamanho do frame for menor que o mínimo, preenche com zeros
            unsigned char padded_buffer[MIN_ETH_PAYLOAD_SIZE] = {0};
            memcpy(padded_buffer, send_buffer, bytes_to_send);
            bytes = sendto(sockfd, padded_buffer, MIN_ETH_PAYLOAD_SIZE, 0, (struct sockaddr*)destino, sizeof(struct sockaddr_ll));
        } else {                                            // Se o tamanho do frame for maior ou igual ao mínimo, envia normalmente
            bytes = sendto(sockfd, send_buffer, bytes_to_send, 0, (struct sockaddr*)destino, sizeof(struct sockaddr_ll));
        }

        if (bytes < 0) {
            perror("send/sendto");
        } else {
            printf("[%s] Frame enviado (seq=%u, tipo=%u)\n", modo_servidor ? "Servidor" : "Cliente", seq, tipo);
        }

        int resultado = espera_resposta(sockfd, seq, timeout);

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
    free(f);
    return 0; // Falha ao enviar a mensagem após o número máximo de tentativas

}

void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr) {
    unsigned char buffer[2048];

    while (1) {

        struct sockaddr_ll addr;
        socklen_t addrlen = sizeof(addr);
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
        int tam_buffer = bytes;
        remove_vlan(buffer, &tam_buffer);
        if (bytes < 5) continue;                                                            // Verifica se o tamanho mínimo do frame é atendido
        printf("[Servidor DEBUG 1] Recebidos %zd bytes.\n", bytes);
        for (ssize_t i = 0; i < bytes - 4;) {                                               // Percorre o buffer procurando por frames válidos
            printf("[Servidor DEBUG 2] Verificando byte %zd do buffer...\n", i);
            if (buffer[i] == MARCADOR) {                                                    // Verifica se o marcador é válido
                printf("[Servidor DEBUG 3] MARCADOR (0x7E) encontrado!\n");

                Frame f;

                unsigned char tipo = (buffer[i+1] >> 4) & 0x0F;
                unsigned char seq = ((buffer[i+1] & 0x0F) << 1) | ((buffer[i+2] >> 7) & 0x01);
                unsigned char tam = buffer[i+2] & 0x7F;
                unsigned char checksum_recebido = buffer[i+3];

                f.tipo = tipo;
                f.seq = seq;
                f.tamanho = tam;
                memcpy(f.dados, &buffer[4], tam);

                unsigned char checksum_calculado = calcula_checksum_manual(&buffer[i], f.tamanho);

                if (checksum_recebido != checksum_calculado) {
                    printf("[%s] Checksum inválido seq=%u\n", modo_servidor ? "Servidor" : "Cliente", f.seq);
                    envia_resposta(sockfd, f.seq, 1, &addr, NULL); // Envia NACK
                    i++;
                    continue;
                }
                
                printf("[Servidor DEBUG 6] Checksum VÁLIDO. Processando tipo %u\n", f.tipo);
                int is_duplicate = 0;
                if (f.tipo >= 10 && f.tipo <= 13 && f.seq == last_seq) {
                    is_duplicate = 1;
                    printf("[Servidor] Duplicado de movimento recebido (seq=%u).\n", f.seq);
                }

                //unsigned char tipo = f.tipo;

                if (modo_servidor) {

                    if (tipo == 0 || tipo == 1 || tipo == 2) {                              // Ignora ACK, NACK ou OK+ACK
                        i += 4 + f.tamanho;
                        continue;
                    }

                    if (tipo == 3) {                                                       // Tipo 3 = Jogo encerrado
                        printf("[Servidor] Jogo encerrado\n");
                        envia_resposta(sockfd, f.seq, 0, &addr, NULL);
                        return;
                    }

                    if (tipo == 14) {                                                       // Tipo 14 = Jogo em andamento
                        if (steps_taken > 0) {                                              // Se já houver passos dados, não permite iniciar o jogo
                            printf("[Servidor] Jogo ja iniciado, reiniciando...\n");
                            unsigned char error_code[2] = {'3', '\0'};
                            envia_resposta(sockfd, f.seq, 15, &addr, error_code);          // ERRO TIPO 3
                            return;
                        }
                        else {                                                              // Se não houver passos dados, inicia o jogo
                            printf("[Servidor] Jogo iniciado\n");
                            envia_resposta(sockfd, f.seq, 0, &addr, NULL); // ACK
                        }
                    }

                    // Processa movimento
                    if (!is_duplicate) {
                        if (tipo == 11) update_x('-', current_pos);
                        else if (tipo == 12) update_x('+', current_pos);
                        else if (tipo == 13) update_y('-', current_pos);
                        else if (tipo == 10) update_y('+', current_pos);

                        add_move(current_pos);                                                  // Adiciona o movimento à lista de movimentos
                        steps_taken++;
                        last_seq = f.seq;
                    }

                    int tesouro = treasure_found(tesouros, current_pos->x, current_pos->y); // Verifica se um tesouro foi encontrado

                    if (tesouro >= 0) {

                        envia_resposta(sockfd, f.seq, 2, &addr, NULL);                        // OK+ACK
                        if (!is_duplicate) {
                            tesouros[tesouro].encontrado = 1;                                  // Marca o tesouro como encontrado
                            print_info(tesouros);
                            treasures_found++;
                        }

                        unsigned char nome[64];
                        unsigned char file_path[90];
                        int tipo_arq = find_file(tesouros[tesouro].id, nome, sizeof(nome), file_path, sizeof(file_path));

                        if (tipo_arq >= 0) {

                            if (access((char*)file_path, R_OK) == -1) {                       // Verifica se o arquivo pode ser lido
                                perror("[Servidor] Erro de permissão de acesso ao arquivo");
                                unsigned char error_code[2] = {'0', '\0'};
                                envia_resposta(sockfd, f.seq, 15, &addr, error_code);
                                i += 4 + f.tamanho;
                                continue;

                            }
                            
                            printf("[Servidor] Arquivo encontrado para o tesouro %d: %s\n", tesouros[tesouro].id, nome);
                            struct stat filestat;
                            if (stat((char*)file_path, &filestat) != 0) {                                                       // Obtém informações do arquivo
                                perror("[Servidor] Erro ao obter informações do arquivo");
                                return;
                            }

                            unsigned char tamanho_str[16];
                            printf("[Servidor] st_size lido de stat(): %ld\n", (long)filestat.st_size);
                            snprintf(tamanho_str, sizeof(tamanho_str), "%ld", (long)filestat.st_size);

                            int authorized = envia_mensagem(sockfd, f.seq, 4, (unsigned char*)tamanho_str, strlen(tamanho_str), 1, &addr);

                            if (authorized == 1) {                                               // Se o cliente autorizou o recebimento do arquivo      

                                unsigned char next_seq = (f.seq + 1) % 32;
                                printf("[Servidor] Enviando arquivo %s com extensão %s\n", nome, extensoes[tipo_arq]);
                                int name_sent = envia_mensagem(sockfd, next_seq, tipo_arq + 6, nome, strlen((char*)nome), 1, &addr);

                                if (name_sent == 1) { // Se o nome do arquivo foi enviado com sucesso
                                    next_seq = (next_seq + 1) % 32;
                                    servidor_envia_arquivo(sockfd, next_seq, (char*)file_path, &addr);
                                } else if (name_sent == 0) { // Se não foi possível enviar o nome do arquivo
                                    printf("[Servidor] Falha ao enviar nome do arquivo %s\n", nome);
                                    return; // Assumindo que o cliente não está mais ativo
                                }
                            } else if (authorized == 4) { // Se o cliente não tem espaço suficiente
                                printf("[Servidor] Cliente não tem espaço suficiente para o arquivo\n");
                                i += 4 + f.tamanho; // Vai p/ o próximo frame
                                continue;
                            } else if (authorized == 0) { // Resposta não recebida, timeouts ou nacks excessivos
                                printf("[Servidor] Resposta nao recebida\n");
                                return; //Assume que o cliente não está mais ativo
                            }

                            if (treasures_found >= 8) {                                          // Se todos os tesouros foram encontrados
                                printf("[Servidor] Todos os tesouros encontrados! Jogo encerrado.\n");
                                return;
                            }

                        }
                        else {                                                              // Se o arquivo não foi encontrado
                            printf("[Servidor] Arquivo não encontrado para o tesouro %d\n", tesouros[tesouro].id);
                            unsigned char error_code[2] = {'2', '\0'};
                            envia_resposta(sockfd, f.seq, 15, &addr, error_code);
                        }
                    } else {                                                                // Se não foi encontrado nenhum tesouro
                        print_info(); 
                        envia_resposta(sockfd, f.seq, 0, &addr, NULL); // ACK
                    }

                } else {

                    if (tipo >= 6 && tipo <= 8) {
                        f.dados[f.tamanho] = '\0';
                        unsigned char file_name[140];
                        snprintf((char*)file_name, sizeof(file_name), "%s%s", (char*)f.dados, (char*)extensoes[tipo - 6]);
                        printf("[Cliente] Nome do arquivo recebido: %s\n", file_name);

                        envia_resposta(sockfd, f.seq, 0, &addr, NULL); // CORRECT

                        unsigned char next_seq = (f.seq + 1) % 32;
                        if (cliente_recebe_arquivo(sockfd, (char*)file_name, next_seq, &addr)) {
                            return;                                                                     // Arquivo recebido com sucesso
                        }

                    } else if (tipo == 4) {

                        printf("[Cliente] Tamanho do arquivo recebido: %s bytes\n", f.dados);
                        long file_size = strtol((char*)f.dados, NULL, 10);
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
                            envia_resposta(sockfd, f.seq, 0, &addr, NULL); // CORRECT
                        } else {
                            printf("[Cliente] Espaço insuficiente (%lld bytes livres)\n", livre);
                            unsigned char error_code[2] = {'1', '\0'};
                            envia_resposta(sockfd, f.seq, 15, cliente_addr, error_code); // tipo 15 = erro
                            return;
                        }

                    } else if (tipo == 15) {
                        if (strcmp((char*)f.dados, "2") == 0)
                            printf("Arquivo nao encontrado, Error Code: %s\n", f.dados);
                        else if (strcmp((char*)f.dados, "0") == 0)
                            printf("Permissao negada, Error Code: %s\n", f.dados);
                        return;
                    } else {
                        printf("[Cliente] Tipo inesperado %u\n", tipo);
                    }
                }

                i += 4 + f.tamanho;
            } else {
                i++;
            }
        }
    }
}

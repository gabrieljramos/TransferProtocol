#include "manager.h"

bool is_in_interval (int value, int init)   //verifica se a sequencia de interresse esta no intervalo da janela atual
{
    for (int i = 0; i < WINDOW_SIZE; i++)
        if (value == (init + i) % SEQ_SIZE)
            return true;

    return false;
}

// int choice (int value, int current){
//     if (current <= 0)
//         return 
// }

void Manager::avanca_janela()
{
    Pacote *packet = fila_envio.dequeue();
    free(packet);
}

void Manager::avanca_n_janela(int i)
{
    for (int j = 0; j < (i - 1); j++)
    {
        avanca_janela();
    }
}

void Manager::ajusta_janela(int resposta)
{
    int i = 0;
    if (resposta < 0)
        i = (resposta * -1) - 1;
    else
        i = resposta;

    avanca_n_janela(i);
}

void Manager::update_sys_time ()
{
    sys_time = timestamp();
}

long long Manager::get_sys_time()
{
    return sys_time;
}

void Manager::add_package(Frame* data)
{
    Pacote pacote;
    pacote.monta_frame(data);
    pacote.set_timer(0, 0);
    fila_envio.enqueue(&pacote);
}

TimeControl* Manager::get_reference(){
    return reference;
}

void Manager::set_reference(TimeControl *reference){
    this->reference = reference;
}

////////////////////////////////////////////////////////////////////////////////

int Manager::espera_ack(int sockfd, unsigned char seq_esperado, int timeoutMillis) { //sempre atualizar o esperado!
                                                                                    //precisa ser o inicio da janela!
    Frame reposta;

    //Define timeout
    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    update_sys_time();
    long long inicio = get_sys_time();

    while (timestamp() - inicio <= timeoutMillis) {
        ssize_t n = recv(sockfd, &resposta, sizeof(resposta), 0);
        if (n > 0 && resposta.marcador == MARCADOR) {
            printf("[Cliente] Recebido %zd bytes\n", n);
            printf("[Cliente] Dados recebidos: ");
            for (int i = 0; i < n; i++) printf("%02X ", ((unsigned char*)&resposta)[i]);
            printf("\n");

            if (!is_in_interval(resposta.seq, seq_esperado)){
                printf("[Cliente] Sequencia invalida. Esperado:%u, Recebido:%u\n", seq_esperado, resposta.seq);
                return NACK1;
            }

            switch (resposta.tipo)
            {
            case 0: //ACK
                printf("[Cliente] ACK recebido para seq=%u\n", resposta.seq);
                return resposta.seq;
                break;
            case 1: //NACK
                printf("[Cliente] NACK recebido para seq=%u\n", resposta.seq);
                return -1 * resposta.seq;
                break;
            case 2: //ACK + OK
                printf("[Cliente] Tesouro recebido para seq=%u\n", resposta.seq);
                return resposta.seq;   //aqui fica como???
                break;
            case 15:
                if (strcmp((char*)resposta.dados, "0") == 0) {              // Erro de permissao
                    printf("[Servidor] Sem permissao de leitura\n");
                    return ERROR1;
                }
                else if (strcmp((char*)resposta.dados, "1") == 0) {              // Erro de espaço insuficiente
                    printf("[Servidor] Espaco insuficiente para o arquivo\n");
                    return ERROR2;
                }
                else if (strcmp((char*)resposta.dados, "3") == 0) {         // Erro de jogo já iniciado
                    printf("[Servidor] Jogo ja iniciado, reiniciando...\n");
                    return ERROR3;
                }
                break;
            default:
                break;
            }    
        }else {
            printf("[Cliente] Timeout esperando ACK/NACK para seq=%u\n", seq_esperado);
            return 0;
        }
    }
}

void envia_resposta(int sockfd, unsigned char seq, unsigned char tipo, struct sockaddr_ll* origem, unsigned char *msg) {

    Frame resposta;

    if (msg == NULL)    //ajustes aqui!
        monta_frame(&resposta, seq, tipo, NULL, 0);
    else
        monta_frame(&resposta, seq, tipo, msg, strlen((char*)msg));

    size_t bytes_to_send = FRAME_HEADER_SIZE + resposta.tamanho;
    int bytes_sent;

    if (bytes_to_send < MIN_ETH_PAYLOAD_SIZE) {   // Se o tamanho do frame for menor que o mínimo, preenche com zeros
        unsigned char padded_buffer[MIN_ETH_PAYLOAD_SIZE] = {0};
        memcpy(padded_buffer, &resposta, bytes_to_send);
        resposta = padded_buffer;
        bytes_to_send = MIN_ETH_PAYLOAD_SIZE;
    }

    if (origem != NULL) {
        bytes_sent = sendto(sockfd, &resposta, bytes_to_send, 0, (struct sockaddr*)origem, sizeof(struct sockaddr_ll));
    } else {
        bytes_sent = send(sockfd, &resposta, bytes_to_send, 0);
    }

    //send(sockfd, &resposta, bytes_to_send, 0);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Manager::envia_fila(int sockfd, struct sockaddr_ll* destino){
    Pacote *lista[WINDOW_SIZE], *aux;
    Frame *aux;
    TimeControl *timer;

    if (fila_envio.empty())
        return;

    for (int i = 0; i < WINDOW_SIZE; i++){
        lista[i] = fila_envio.get_object(i);
        if (lista[i]->get_timer().time == 0)
            lista[i]->set_timer(timestamp(), lista[i]->get_tentativas() * TIMEOUT_MILLIS);
    }

    set_reference(&(lista[0]->get_timer()));    //referencia o timer do 1 item no quadro

    while (lista[0] && lista[0]->get_tentativas() < MAX_RETRANSMISSIONS && !fila_envio.empty())
    {
        for (int i = 0; i < WINDOW_SIZE; i++){
            tenta_enviar(lista[i], sockfd, destino);
        }

        int resultado = espera_ack(sockfd, lista[0]->get_frame()->seq, reference->delay);

        for (int i = 0; i < WINDOW_SIZE; i++){  //update na situação dos quadros escolhidos
            aux = lista[i];
            aux->set_tentativas(aux->get_tentativas() + 1);
            aux->set_timer(aux->get_tentativas() * TIMEOUT_MILLIS);
        }

        ajusta_janela(resultado);

        //aqui escuto mensagem de tesouro recebida? o que e isso?

        for (int i = 0; i < WINDOW_SIZE; i++){
            lista[i] = fila_envio.get_object(i);
            if (lista[i]->get_timer().time == 0)
                lista[i]->set_timer(timestamp(), TIMEOUT_MILLIS);
        }

        set_reference(&(lista[0]->get_timer()));    //referencia o timer do 1 item no quadro
    }

    if (!fila_envio.empty())
        printf("[%s] Falha após %d tentativas no seq=%u\n", "Servidor", MAX_RETRANSMISSIONS, lista[0]->get_frame()->seq);
}



void tenta_enviar (Pacote* package, int sockfd, struct sockaddr_ll* destino){
    if (!package)
        return;

    Frame *aux = package->get_frame();

    sendto(sockfd, aux, aux->tamanho, 0, (struct sockaddr *)destino, sizeof(struct sockaddr_ll));
}

void Manager::escuta_mensagem(int sockfd, tes_t *tesouros, coord_t *current_pos, struct sockaddr_ll *cliente_addr){
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
                    envia_resposta(sockfd, f->seq, 1, &addr, NULL); // Envia NACK
                    i++;
                    continue;
                }

                unsigned char tipo = f->tipo & 0x0F;                                        // Extrai o tipo do frame

                if (modo_servidor) {

                    if (tipo == 0 || tipo == 1 || tipo == 2) {                              // Ignora ACK, NACK ou OK+ACK
                        i += FRAME_HEADER_SIZE + f->tamanho;
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

                    add_move(current_pos);                                                  // Adiciona o movimento à lista de movimentos
                    steps_taken++;

                    int tesouro = treasure_found(tesouros, current_pos->x, current_pos->y); // Verifica se um tesouro foi encontrado

                    if (tesouro >= 0) {

                        envia_resposta(sockfd, f->seq, 2, &addr, NULL);                     // OK+ACK
                        tesouros[tesouro].encontrado = 1;                                  // Marca o tesouro como encontrado
                        print_info(tesouros);
                        treasures_found++;

                        unsigned char nome[64];
                        unsigned char file_path[90];
                        int tipo_arq = find_file(tesouros[tesouro].id, nome, sizeof(nome), file_path, sizeof(file_path));

                        if (tipo_arq >= 0) {

                            if (access((char*)file_path, R_OK) == -1) {                       // Verifica se o arquivo pode ser lido
                                perror("[Servidor] Erro de permissão de acesso ao arquivo");
                                unsigned char error_code[2] = {'0', '\0'};
                                envia_resposta(sockfd, f->seq, 15, &addr, error_code);
                                i += FRAME_HEADER_SIZE + f->tamanho;
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

                            int authorized = envia_mensagem(sockfd, f->seq, 4, (unsigned char*)tamanho_str, strlen(tamanho_str), 1, &addr);

                            if (authorized) {                                               // Se o cliente autorizou o recebimento do arquivo      

                                unsigned char next_seq = (f->seq + 1) % 32;
                                printf("[Servidor] Enviando arquivo %s com extensão %s\n", nome, extensoes[tipo_arq]);
                                int name_sent = envia_mensagem(sockfd, next_seq, tipo_arq + 6, nome, strlen((char*)nome), 1, &addr);

                                if (name_sent) {
                                    next_seq = (next_seq + 1) % 32;
                                    servidor_envia_arquivo(sockfd, next_seq, (char*)file_path, &addr);
                                }
                            }

                            if (treasures_found >= 8) {                                          // Se todos os tesouros foram encontrados
                                printf("[Servidor] Todos os tesouros encontrados! Jogo encerrado.\n");
                                return;
                            }

                        }
                        else {                                                              // Se o arquivo não foi encontrado
                            printf("[Servidor] Arquivo não encontrado para o tesouro %d\n", tesouros[tesouro].id);
                            unsigned char error_code[2] = {'2', '\0'};
                            envia_resposta(sockfd, f->seq, 15, &addr, error_code);
                        }
                    } else {                                                                // Se não foi encontrado nenhum tesouro
                        print_info(); 
                        envia_resposta(sockfd, f->seq, 0, &addr, NULL); // ACK
                    }

                } else {

                    if (tipo >= 6 && tipo <= 8) {

                        unsigned char file_name[140];
                        snprintf((char*)file_name, sizeof(file_name), "%s%s", (char*)f->dados, (char*)extensoes[tipo - 6]);
                        printf("[Cliente] Nome do arquivo recebido: %s\n", file_name);

                        envia_resposta(sockfd, f->seq, 0, NULL, NULL);

                        unsigned char next_seq = (f->seq + 1) % 32;
                        if (cliente_recebe_arquivo(sockfd, (char*)file_name, next_seq)) {
                            return;                                                                     // Arquivo recebido com sucesso
                        }

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

                    } else if (tipo == 15) {
                        if (strcmp((char*)f->dados, "2") == 0)
                            printf("Arquivo nao encontrado, Error Code: %s\n", f->dados);
                        else if (strcmp((char*)f->dados, "0") == 0)
                            printf("Permissao negada, Error Code: %s\n", f->dados);
                        return;
                    } else {
                        printf("[Cliente] Tipo inesperado %u\n", tipo);
                    }
                }

                i += FRAME_HEADER_SIZE + f->tamanho;
            } else {
                i++;
            }
        }
}
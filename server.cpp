int servidor_envia_arquivo(int sockfd, unsigned char seq, const char* file_path, struct sockaddr_ll* destino) {
    //AQUI VOU EMPACOTAR PRA DEDEU!!!

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
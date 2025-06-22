#include "client.h"

int Client::recebe_arquivo(int sockfd, const char* file_name, unsigned char seq) {

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
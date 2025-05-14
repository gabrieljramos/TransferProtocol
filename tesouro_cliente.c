#include "common.h"

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
            } else if (resposta.tipo == 6 && resposta.seq == seq_esperado) { // OK + ACK
                printf("[Cliente] Texto recebido para seq=%u\n", seq_esperado);
                printf("File: %s\n", resposta.dados);
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
void envia_mensagem(int sockfd, const char *interface, unsigned char seq, unsigned char tipo) {
    Frame f;
    memset(&f, 0, sizeof(Frame));
    monta_frame(&f, seq, tipo, NULL, tipo);

    int timeout = TIMEOUT_MILLIS;
    int tentativas = 0;
    int ack_ok = 0;

    while (tentativas < MAX_RETRANSMISSIONS && !ack_ok) {

        int bytes = send(sockfd, &f, sizeof(Frame), 0);
        if (bytes < 0) 
            perror("sendto");
        else 
            printf("[Cliente] Frame enviado (seq=%u)\n", seq);

        int resultado = espera_ack(sockfd, seq, timeout);

        if (resultado == 1) //Caso o ack tenha chego com sucesso
            ack_ok = 1;
        else if (resultado == 2) { //Caso ACK tenha chego com sucesso e encontrado um tesouro
            ack_ok = 1;
            resultado = espera_ack(sockfd, seq, timeout);
        }
        else { //timeout/NACK
            tentativas++;
            timeout *= 2;
        }
    }
    if (!ack_ok) { //Caso num de tentativa tenha excedido o maximo
        printf("[Cliente] Falha após %d tentativas\n", MAX_RETRANSMISSIONS);
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    //Salva a posicao atual
    coord_t current_pos;
    current_pos.x = 7;
    current_pos.y = 0;

    //Salva a interface e inicia o socket
    const char *interface = argv[1];
    int sockfd = cria_raw_socket(interface);

    //Inicializa a sequencia
    int seq = 0;

    //Cria e inicializa a matriz do mapa
    char map[8][8];
    start_map(map);

    //Imprime o mapa
    print_map(map);

    //Variavel para salvar o movimento escolhido
    int game_state;

    //Loop principal do jogo
    while (game_state != QUIT) {
        game_state = menu();
        switch(game_state) {
            case UP:
                envia_mensagem(sockfd, interface, seq, 11);
                update_x('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case LEFT:
                envia_mensagem(sockfd, interface, seq, 13);
                update_y('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case DOWN:
                envia_mensagem(sockfd, interface, seq, 12);
                update_x('+',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case RIGHT:
                envia_mensagem(sockfd, interface, seq, 10);
                update_y('+',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case QUIT:
                break;
            default:
                printf("Digite uma opção válida (1..5)\n");
                break;
        }
    }
    
    close(sockfd);
    return 0;
}


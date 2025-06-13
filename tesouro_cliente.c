#include "common.h"

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    const char *interface = argv[1];
    int sockfd = cria_raw_socket(interface);                                // Cria um raw_socket para a interface especificada

    int seq = 0;                                                            // Inicializa a sequência de pacotes
    int game_state = envia_mensagem(sockfd, seq, 14, NULL, 0, 0, NULL);     // Confirma se ha um jogo em andamento

    seq = (seq + 1) % 32;                                                   // Incrementa a sequência de pacotes

    coord_t current_pos;                                                    // Inicializa a posição inicial do jogador
    current_pos.x = 7;
    current_pos.y = 0;

    char map[8][8];
    start_map(map);                                                         // Inicializa o mapa do jogo

    print_map(map);                                                         // Imprime o mapa inicial

    while (game_state != QUIT) {                                            // Enquanto o jogo não for encerrado
        game_state = menu();
        switch(game_state) {
            case UP:
                envia_mensagem(sockfd, seq, 11, NULL, 0, 0, NULL);          // Envia mensagem de movimento para cima
                update_x('-',&current_pos);                                 // Atualiza a posição do jogador para cima
                map[current_pos.x][current_pos.y] = '-';                    // Marca a nova posição no mapa
                print_map(map);                                             // Imprime o mapa atualizado
                seq = (seq + 1) % 32;                                       // Incrementa a sequência de pacotes
                break;
            case DOWN:
                envia_mensagem(sockfd, seq, 12, NULL, 0, 0, NULL);          // Envia mensagem de movimento para baixo
                update_x('+',&current_pos);                                 // Atualiza a posição do jogador para baixo
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case LEFT:
                envia_mensagem(sockfd, seq, 13, NULL, 0, 0, NULL);          // Envia mensagem de movimento para a esquerda
                update_y('-',&current_pos);                                 // Atualiza a posição do jogador para a esquerda
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case RIGHT:
                envia_mensagem(sockfd, seq, 10, NULL, 0, 0, NULL);          // Envia mensagem de movimento para a direita
                update_y('+',&current_pos);                                 // Atualiza a posição do jogador para a direita
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
    
    close(sockfd);                                                          // Fecha o socket
    return 0;
}


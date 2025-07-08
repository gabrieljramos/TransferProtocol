#include "common.h"

int tesouros_encontrados = 0;                                                // Variável global para contar os tesouros encontrados
int game_state = 0;                                                          // Variável global para o estado do jogo

void make_move(char map[8][8], int sockfd, int seq, coord_t *current_pos, int direction) {

    int result = envia_mensagem(sockfd, seq, direction, NULL, 0, 0, NULL);  // Envia a mensagem de movimento para o servidor

    if (result == 0) {
        printf("Timeout ou erro ao receber ACK/NACK. Tente novamente.\n");
        game_state = QUIT;                                                  // Se não recebeu ACK/NACK, encerra o jogo
        return;
    }

    if (map[current_pos->x][current_pos->y] == '#') {
        map[current_pos->x][current_pos->y] = 'T';                          // Marca a posição atual com T se for um tesouro
    }
    else if (map[current_pos->x][current_pos->y] == '*')
        map[current_pos->x][current_pos->y] = '0';                          // Posicao anterior eh 0

    if (direction == 11) {                                                  // Se a direção for para cima
        update_x('-', current_pos);
    } else if (direction == 12) {                                           // Se a direção for para baixo
        update_x('+', current_pos);
    } else if (direction == 13) {                                           // Se a direção for para a esquerda
        update_y('-', current_pos);
    } else if (direction == 10) {                                           // Se a direção for para a direita
        update_y('+', current_pos);
    }

    if (result == 2) {
        tesouros_encontrados++;
        map[current_pos->x][current_pos->y] = '#';                          // Se um tesouro foi encontrado, marca a posição atual com #
    }
    else {
        if (map[current_pos->x][current_pos->y] == 'T') {
            map[current_pos->x][current_pos->y] = '#';
        } 
        else {
            map[current_pos->x][current_pos->y] = '*';
        }
    }
    print_map(map);  

}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    char *interface = argv[1];
    int sockfd = cria_raw_socket(interface);                                // Cria um raw_socket para a interface especificada

    int seq = 0;                                                            // Inicializa a sequência de pacotes
    game_state = envia_mensagem(sockfd, seq, 14, NULL, 0, 0, NULL);         // Confirma se ha um jogo em andamento
    if (game_state != 1) {
        game_state = QUIT;
    }

    seq = (seq + 1) % 32;                                                   // Incrementa a sequência de pacotes

    coord_t current_pos;                                                    // Inicializa a posição inicial do jogador
    current_pos.x = 7;
    current_pos.y = 0;

    char map[8][8];
    start_map(map);                                                         // Inicializa o mapa do jogo

    print_map(map);                                                         // Imprime o mapa inicial

    while (game_state != QUIT) {                                            // Enquanto o jogo não for encerrado

        if (tesouros_encontrados >= 8) {                                         // Se todos os tesouros foram encontrados
            printf("Todos os tesouros foram encontrados! Jogo encerrado.\n");
            break;
        }

        game_state = menu();
        switch(game_state) {
            case UP:                                     
                make_move(map, sockfd, seq, &current_pos, 11);              // Chama a função para fazer o movimento
                seq = (seq + 1) % 32;
                break;
            case DOWN:
                make_move(map, sockfd, seq, &current_pos, 12);
                seq = (seq + 1) % 32;
                break;
            case LEFT:
                make_move(map, sockfd, seq, &current_pos, 13);
                seq = (seq + 1) % 32;
                break;
            case RIGHT:
                make_move(map, sockfd, seq, &current_pos, 10);                         
                seq = (seq + 1) % 32;
                break;
            case QUIT:
                envia_mensagem(sockfd, seq, 3, NULL, 0, 0, NULL);           // Envia mensagem de encerramento do jogo
                printf("Jogo encerrado.\n");
                break;
            default:
                printf("Digite uma opção válida (1..5)\n");
                break;
        }
    }
    
    close(sockfd);                                                          // Fecha o socket
    return 0;
}

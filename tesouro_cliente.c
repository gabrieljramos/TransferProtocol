#include "common.h"

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

    Frame f;
    memset(&f, 0, sizeof(Frame));

    //Loop principal do jogo
    while (game_state != QUIT) {
        game_state = menu();
        switch(game_state) {
            case UP:
                monta_frame(&f, seq, 11, NULL, 11);
                envia_mensagem(sockfd, seq, 11, NULL, 0, 0, NULL);
                update_x('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case LEFT:
                monta_frame(&f, seq, 13, NULL, 13);
                envia_mensagem(sockfd, seq, 13, NULL, 0, 0, NULL);
                update_y('-',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case DOWN:
                monta_frame(&f, seq, 12, NULL, 12);
                envia_mensagem(sockfd, seq, 12, NULL, 0, 0, NULL);
                update_x('+',&current_pos);
                map[current_pos.x][current_pos.y] = '-';
                print_map(map);
                seq = (seq + 1) % 32;
                break;
            case RIGHT:
                monta_frame(&f, seq, 10, NULL, 10);
                envia_mensagem(sockfd, seq, 10, NULL, 0, 0, NULL);
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


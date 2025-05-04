#include "game.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>


/*

    SERVIDOR:

*/

move_t *move_list = NULL;

//Adiciona um movimento da lista
void add_move(move_t *move) {

    queue_append((queue_t **) &move_list, (queue_t*) move);

}

//Inicializa o jogo
tes_t* game_start() {

    //Sorteia os tesouros e salva suas posicoes
    int treasure_amount = 8;
    tes_t* tesouros = (tes_t*) malloc(sizeof(tes_t)*treasure_amount);
    for (int i = 0; i < treasure_amount; i++) {
        tesouros[i].id = i+1;
        tesouros[i].pos.x = rand() % 8;
        tesouros[i].pos.y = rand() % 8;
    }

    //Cria uma struct para a posicao inicial e salva ela numa lista
    move_t* first_move = malloc(sizeof(move_t));
    first_move->pos.x = 7;
    first_move->pos.y = 0;
    first_move->next = NULL;
    first_move->prev = NULL;

    add_move(first_move);

    return tesouros;

}

int get_game_x(int x) {
    return 7 - x;
}

//Verifica se um tesouro foi encontrado
int treasure_found(tes_t *treasures, int x, int y) {

    for (int i = 0; i < 8; i++) {
        if (treasures[i].pos.x == x && treasures[i].pos.y == y)
            return i;
    }
    return -1;

}


/*

    CLIENTE:

*/

//Imprime um menu com opcoes de jogadas na tela e retorna a opcao escolhida
int menu() {

    int game_state;
    printf("[1] - UP\n[2] - LEFT\n[3] - DOWN\n[4] - RIGHT\n[5] - QUIT\n");
    scanf("%d", &game_state);
    return game_state;

}

//Imprime a matriz do mapa
void print_map(char map[8][8]) {

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
            printf("%c ",map[i][j]);
        printf("\n");
    }

}

//Inicializa a matriz do mapa
void start_map(char map[8][8]) {

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if ((i == 7) && (j == 0))
                map[i][j] = '-';
            else
                map[i][j] = 'X';
        }
    }

}

/*

    COMPARTILHADAS

*/

void update_x(char dir, coord_t *pos) {
    if (dir == '+') pos->x = (pos->x >= 6) ? pos->x - 6 : pos->x + 2;
    else if (dir == '-') pos->x = (pos->x <= 1) ? pos->x + 6 : pos->x - 2;
}

void update_y(char dir, coord_t *pos) {
    if (dir == '+') pos->y = (pos->y >= 6) ? pos->y - 6 : pos->y + 2;
    else if (dir == '-') pos->y = (pos->y <= 1) ? pos->y + 6 : pos->y - 2;
}

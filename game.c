#include "game.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>


/*

    SERVIDOR:

*/

move_t *move_list = NULL;
tes_t *treasure_list = NULL;

void print_move (void *ptr)
{
    move_t *elem = ptr ;

    if (!elem)
        return ;

    printf(" (%d, %d)", (7 - elem->pos.x), elem->pos.y); // Imprime a posição do movimento, 7 - x para o canto inferior esquerdo ser 0,0

}

void print_treasure (tes_t *treasures)
{

    printf("Treasure List: ");
    printf("[ ");
    for (int i = 0; i < 8; i++) {
        if (treasures[i].encontrado) {
            printf("{(%d, %d):%d}", (7 - treasures[i].pos.x), treasures[i].pos.y, treasures[i].id);
        }
    }
    printf(" ]\n");

}

void print_info(tes_t *treasures) {

    print_treasure(treasures); // Imprime os tesouros encontrados
    queue_print("Move List", (queue_t*)move_list, print_move);
}


//Adiciona um movimento da lista
void add_move(coord_t* current_pos) {

    move_t *mv = malloc(sizeof(move_t));                                    // Aloca memória para o movimento
    mv->pos = *current_pos;                         
    mv->next = mv->prev = NULL;
    queue_append((queue_t **) &move_list, (queue_t*) mv);


}

int tesouro_igual(tes_t *treasures, int x, int y, int max) {

    for (int i = 0; i < max; i++) {
        if (treasures[i].pos.x == x && treasures[i].pos.y == y) {
            return 1; // Tesouro já existe na posição
        }
    }
    return 0; // Tesouro não existe na posição
}

//Inicializa o jogo
tes_t* game_start() {

    //Sorteia os tesouros e salva suas posicoes
    int treasure_amount = 8;
    tes_t* tesouros = (tes_t*) malloc(sizeof(tes_t)*treasure_amount);
    for (int i = 0; i < treasure_amount; i++) {
        tesouros[i].id = i+1;
        tesouros[i].encontrado = 0;                                 // Inicializa como não encontrado
        tesouros[i].pos.x = rand() % 8;
        tesouros[i].pos.y = rand() % 8;
        while (tesouro_igual(tesouros, tesouros[i].pos.x, tesouros[i].pos.y, i)) {
            tesouros[i].pos.x = rand() % 8;                       // Gera nova posição se já existir um tesouro na posição
            tesouros[i].pos.y = rand() % 8;
        }
    }

    return tesouros;

}

int get_game_x(int x) {
    return 7 - x;
}

//Verifica se um tesouro foi encontrado
int treasure_found(tes_t *treasures, int x, int y) {

    for (int i = 0; i < 8; i++) {
        if (treasures[i].pos.x == x && treasures[i].pos.y == y && treasures[i].encontrado == 0) {
            treasures[i].encontrado = 1; // Marca o tesouro como encontrado
            return i;
        }
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
                map[i][j] = '*';
            else
                map[i][j] = 'X';
        }
    }

}

void update_map(char map[8][8], coord_t pos, int treasure_id) {

    //Atualiza a matriz do mapa com a posicao atual do jogador
    map[pos.y][pos.x] = 'O';

    //Se um tesouro foi encontrado, atualiza o mapa com o id do tesouro
    if (treasure_id != -1) {
        map[pos.y][pos.x] = '0' + treasure_id;
    }

}

/*

    COMPARTILHADAS

*/

void update_x(char dir, coord_t *pos) {
    if (dir == '+') {
        // Se x for 7, volta para 0. Caso contrário, incrementa 1.
        pos->x = (pos->x >= 7) ? pos->x - 7 : pos->x + 1;
    } else if (dir == '-') {
        // Se x for 0, volta para 7. Caso contrário, decrementa 1.
        pos->x = (pos->x <= 0) ? pos->x + 7 : pos->x - 1;
    }
}

void update_y(char dir, coord_t *pos) {
    if (dir == '+') {
        // Se y for 7, volta para 0. Caso contrário, incrementa 1.
        pos->y = (pos->y >= 7) ? pos->y - 7 : pos->y + 1;
    } else if (dir == '-') {
        // Se y for 0, volta para 7. Caso contrário, decrementa 1.
        pos->y = (pos->y <= 0) ? pos->y + 7 : pos->y - 1;
    }
}

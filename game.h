#ifndef __GAME__
#define __GAME__

#define UP 1
#define LEFT 2
#define DOWN 3
#define RIGHT 4
#define QUIT 5

typedef struct coordenadas {
    int x,y;
} coord_t;

typedef struct tesouro {
    int id;
    int encontrado;
    coord_t pos;
} tes_t;

typedef struct movimentos {
    struct movimentos *prev;
    struct movimentos *next;
    coord_t pos;
} move_t;

extern move_t *move_list;

//SERVIDOR:
tes_t* game_start();
void add_move(coord_t* current_pos);
void add_treasure(coord_t* current_pos, int treasure_id);
int treasure_found(tes_t *treasures, int x, int y); 
void print_info();

void update_x(char dir, coord_t *pos);
void update_y(char dir, coord_t *pos);

//CLIENTE:
int menu();
void print_map(char map[8][8]);
void start_map(char map[8][8]);

#endif

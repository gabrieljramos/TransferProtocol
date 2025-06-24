#include "common.h"

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    tes_t* tesouros = (tes_t*) malloc(sizeof(tes_t) * 8);                   // Aloca memória para 8 tesouros
    tesouros = game_start();                                                // Inicia o jogo e obtém a lista de tesouros

    //Imprime lista de tesouros (para teste apenas)
    printf("Tesouros:\n");
    for (int i = 0; i < 8; i++) {
        printf("(%d,%d)\n", ( 7 - tesouros[i].pos.x),tesouros[i].pos.y);
    }

    coord_t current_pos;                                                    // Inicializa a posição inicial do jogador                                  
    current_pos.x = 7;
    current_pos.y = 0;

    const char* interface = argv[1];
    int sockfd = cria_raw_socket(interface);                                // Cria um raw_socket para a interface especificada

    escuta_mensagem(sockfd, 1, tesouros, &current_pos, NULL);               // Escuta mensagens do cliente
    close(sockfd);      
    free(tesouros);                                                    // Fecha o socket
    return 0;
}

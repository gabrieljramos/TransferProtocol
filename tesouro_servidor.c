#include "common.h"

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Uso: %s <interface>\n", argv[0]);
        return 1;
    }

    //Aloca um vetor p/ tesouros e inicia o jogo
    tes_t* tesouros = (tes_t*) malloc(sizeof(tes_t) * 8);
    tesouros = game_start();

    //Imprime lista de tesouros (para teste apenas)
    printf("Tesouros:\n");
    for (int i = 0; i < 8; i++) {
        printf("(%d,%d)\n", tesouros[i].pos.x,tesouros[i].pos.y);
    }
    coord_t current_pos;
    //Salva a posicao inicial
    current_pos.x = 7;
    current_pos.y = 0;

    //Salva a interface e inicia o socket
    const char* interface = argv[1];
    int sockfd = cria_raw_socket(interface);

    //Comeca a esperar por msgs
    //escuta_mensagem(sockfd,tesouros);
    escuta_mensagem(sockfd, 1, tesouros, &current_pos, NULL);
    close(sockfd);
    return 0;
}

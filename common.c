#include "common.h"

#define FRAME_HEADER_SIZE (sizeof(Frame) - 127)
#define MIN_ETH_PAYLOAD_SIZE 14

unsigned char *extensoes[] = {".txt", ".mp4", ".jpg"};

int steps_taken = 0;
int treasures_found = 0;

long long timestamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000LL + tp.tv_usec / 1000;
}

int cria_raw_socket(const char *interface) {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_ll sa = {0};
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = ifr.ifr_ifindex;

    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}


int find_file(int file_num, unsigned char *file_name, size_t name_size, unsigned char *file_path, size_t path_size) {

    for (int i = 0; i < 3; i++) {
        snprintf(file_name, name_size, "%d", file_num);                                                 // Converte o número do arquivo para string
        snprintf(file_path, path_size, "./objetos/%s%s", file_name, extensoes[i]);                      // Concatena o caminho do arquivo com a extensão correspondente
        FILE *fp = fopen(file_path, "rb");

        if (fp) {
            printf("Arquivo encontrado: %s Tamanho(nome): %ld\n", file_name, strlen(file_name));
            fclose(fp);
            return i;                                                                                   // Retorna o índice da extensão encontrada                                      
        }
    }
    return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////

void escuta_mensagem(int sockfd, int modo_servidor, tes_t* tesouros, coord_t* current_pos, struct sockaddr_ll* cliente_addr) {
    
}
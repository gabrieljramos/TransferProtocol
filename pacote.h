#ifndef __PACOTE__
#define __PACOTE__

#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <sys/time.h> // para timeout
#include <sys/stat.h>
#include <stdint.h>
#include <sys/statvfs.h>

#define MARCADOR 0x7E
#define MAX_DADOS 127

typedef struct {
    unsigned char marcador;
    unsigned int  tamanho : 7;
    unsigned int  seq     : 5;
    unsigned int  tipo    : 4;
    unsigned char checksum;
    unsigned char dados[MAX_DADOS];
} __attribute__((packed)) Frame;

typedef struct {
    long long time;
    long long delay;
} TimeControl;

class Pacote {
    private:
        Frame *mensagem;
        TimeControl clock;
        int tentativas;

    public:
        long long timestamp();
        void set_timer(long long current_time, long long how_long);
        void set_timer(long long how_long);
        
        TimeControl get_timer();

        void set_tentativas(int value);
        int get_tentativas();

        Frame *get_frame();
        int cria_raw_socket(const char *interface);
        unsigned char calcula_checksum();

        void monta_frame(unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam);
        void monta_frame(Frame *data);
};

#endif
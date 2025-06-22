#ifndef __MANAGER__
#define __MANAGER__

#include "pacote.h"
#include "fila.h"
#include "common.h"

#define TIMEOUT_MILLIS 300
#define MAX_RETRANSMISSIONS 5

#define WINDOW_SIZE 3
#define SEQ_SIZE 8

#define ACK1 1
#define ACK2 2
#define ACK3 3

#define NACK1 -1
#define NACK2 -2
#define NACK3 -3

#define ERROR1 5    //erro de permissao
#define ERROR2 6    //erro de espaco insuficiente
#define ERROR3 7    //erro de jogo ja iniciado

class Manager{
    private:
        LinkedQueue<Pacote *> fila_envio;
        Pacote fila_recibo[WINDOW_SIZE];
        TimeControl* reference;
        long long sys_time;

    public:
        void avanca_janela();
        void avanca_n_janela(int i);
        void ajusta_janela(int resposta);

        void add_package(Frame* pacote);

        void update_sys_time();
        long long get_sys_time();
        TimeControl* get_reference();
        void set_reference(TimeControl *reference);

        int espera_ack(int sockfd, unsigned char seq_esperado, int timeoutMillis);

        void envia_fila(int sockfd, struct sockaddr_ll* destino);
};

#endif
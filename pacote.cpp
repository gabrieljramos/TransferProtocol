#include "pacote.h"

Frame* Pacote::get_frame()
{
    return mensagem;
}

//Faz a soma de todos os campos necessarios e retorna seu valor
unsigned char Pacote::calcula_checksum() {
    unsigned char sum = 0;
    sum += mensagem->tamanho;
    sum += mensagem->seq;
    sum += mensagem->tipo;
    for (int i = 0; i < mensagem->tamanho; ++i) {
        sum += mensagem->dados[i];
    }
    return sum;
}

//Monta a struct do protocolo
void Pacote::monta_frame(unsigned char seq, unsigned char tipo, unsigned char *dados, size_t tam) {
    mensagem->marcador = MARCADOR;
    mensagem->seq = seq & 0x1F;
    mensagem->tipo = tipo & 0X0F;
    if (tipo == 0 || tipo == 1 || (tipo >= 10 && tipo <= 13)) { //Tipos que nao possuem dados
        mensagem->tamanho = 0;
        memset(mensagem->dados, 0, sizeof(mensagem->dados));
    } else { //Caso tam > 127 trunca tam
        if (tam > 127) tam = 127;
        mensagem->tamanho = tam;
        memcpy(mensagem->dados, dados, tam);
        mensagem->dados[tam] = '\0';
    }
    mensagem->checksum = calcula_checksum();
}

void Pacote::monta_frame(Frame* data){
    mensagem = data;
}

void Pacote::set_timer (long long current_time, long long how_long){
    if (current_time > 0)
        clock.time = current_time;
    else
        clock.time = 0;

    if (how_long > 0)
        clock.delay = how_long;
    else
        clock.delay = 0;
}

void Pacote::set_timer(long long how_long){
    if (how_long > 0)
        clock.delay = how_long;
    else
        clock.delay = 0;
}

TimeControl Pacote::get_timer (){
    return clock;
}

void Pacote::set_tentativas (int value){
    tentativas = value;
}

int Pacote::get_tentativas(){
    return tentativas;
}
/*

Nome: Gabriel Justus Ramos

GRR: 20232348

*/

#include "queue.h"
#include <stdio.h>

void queue_print(char *name, queue_t *queue, void print_elem (void*)) {

    printf("%s: ", name);

    //Fila vazia so imprime []
    if (!queue) {
        printf("[]\n");
        return;
    }

    //Comecamos o loop com o segundo elem para poder falhar na comparacao
    queue_t *aux = (queue)->next;

    printf("[");
    print_elem(queue);                           //Imprime o primeiro elemento
    printf(" ");

    while (aux != queue) {                       //Imprime o resto
        print_elem((void*) aux);
        aux = aux->next;
        if (aux != queue)                        //If para nao imprimir um ' ' dps do ultimo elem
            printf(" ");
    }
    printf("]\n");

}

int queue_size(queue_t *queue) {

    //Fila vazia retorna tamanho = 0
    if (!queue)
        return 0;

    int cont = 1;
    queue_t *temp = queue->next;

    //temp = temp->next;
    while (temp != queue) {
        cont++;
        temp = temp->next;
    }
    return cont;

}

int queue_append(queue_t **queue, queue_t *elem) {

    //Caso elem seja invalido retorna -1
    if ((elem == NULL) || (elem->prev != NULL) || (elem->next != NULL))
        return -1;

    //Fila vazia da append no primeiro lugar
    if (*queue == NULL) {
        *queue = elem;
        (*queue)->next = *queue;
        (*queue)->prev = *queue;
    }
    else { //Caso nao esteja vazia, da append no ultimo lugar
        elem->prev = (*queue)->prev;
        (*queue)->prev->next = elem;
        (*queue)->prev = elem;
        elem->next = *queue;
    }

    return 0;

}

int queue_remove(queue_t **queue, queue_t *elem) {

    if (*queue == NULL || elem == NULL)
        return -1;

    queue_t *aux = *queue;

    if (*queue != elem) {
        queue_t *temp = (*queue)->next;          //Var temporaria para navegar pela fila
        while (temp != elem && temp != *queue)   //Percorre a fila
            temp = temp->next;
        if (temp != elem)
            return -1;                           //elem nao encontrado
        *queue = temp;

    }

    //Remover o unico elem presente na lista
    if ((*queue)->next == *queue && (*queue)->prev == *queue) {
        elem->next = NULL;
        elem->prev = NULL;
        *queue = NULL;                           //Deixa a fila vazia
        return 0;
    }
    else { //Caso contrario
        (*queue)->next->prev = (*queue)->prev;
        (*queue)->prev->next = (*queue)->next;
    }

    //Se remover o primeiro elem, o segundo se torna o primeiro
    if (aux == elem)
        (*queue) = (*queue)->next;
    else
        (*queue) = aux;                          //Retorna o inicio da fila para a posicao correta


    elem->next = NULL;
    elem->prev = NULL;

    return 0;

}

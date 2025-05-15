CC = gcc

# Objetos comuns usados por ambos
COMMON_OBJS = game.o queue.o common.o

# Alvos finais
all: servidor cliente

# Compila o servidor
servidor: tesouro_servidor.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o servidor tesouro_servidor.o $(COMMON_OBJS)

# Compila o cliente
cliente: tesouro_cliente.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o cliente tesouro_cliente.o $(COMMON_OBJS)

# Regras de compilação dos .c em .o
tesouro_servidor.o: tesouro_servidor.c game.h queue.h common.h
	$(CC) $(CFLAGS) -c tesouro_servidor.c

tesouro_cliente.o: tesouro_cliente.c game.h queue.h common.h
	$(CC) $(CFLAGS) -c tesouro_cliente.c

game.o: game.c game.h
	$(CC) $(CFLAGS) -c game.c

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c
# Limpa arquivos objeto e executáveis
clean:
	rm -f *.o servidor cliente


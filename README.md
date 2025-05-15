- Disclaimer: Haha, esqueci q era p/ fazer em C++

- So agr q eu tinha visto q era pra fazer o nosso proprio timeout, mas ta feito ja

- Antes de executar os programas rodar:
  - sudo ip link add veth0 type veth peer name veth1
  - sudo ip link set veth0 up
  - sudo ip link set veth1 up

- Game.c/Game.h: Contem as funcoes relacionadas com o jogo em si, algumas delas sao compartilhadas entre cliente e servidor, enquanto outras sao unicas para cada um

- Tesouro_cliente.c/Tesouro_servidor.c: Bem autoexplicativo

- Queue.c/Queue.h: Como tem q salvar os movimentos feitos pelo jogador, so usei a bibliotecas de fila de SO msm

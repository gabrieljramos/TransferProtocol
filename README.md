# Treasure Hunt Game

This project implements a simple client-server treasure hunt game in C. The communication is done using raw sockets at the Ethernet layer (Layer 2).

## Project Description

The game consists of a server and a client application. The server creates a hidden map with 8 treasures. The client connects to the server and navigates an 8x8 map to find these treasures. When a treasure is found, the server sends a corresponding file to the client.

The communication protocol is custom-built on top of Ethernet frames, handling message sequencing, acknowledgments (ACK/NACK), and retransmissions to ensure reliability.

### Core Components

* **`tesouro_servidor.c`**: The server application that manages the game state, treasure locations, and client interactions.
* **`tesouro_cliente.c`**: The client application that allows a user to play the game by sending movement commands to the server.
* **`game.c` / `game.h`**: Contains the core game logic, such as initializing the game, managing player movement, and tracking found treasures.
* **`common.c` / `common.h`**: Includes shared functions used by both the client and server for network communication, such as creating raw sockets, framing messages, and handling the custom protocol.
* **`queue.c` / `queue.h`**: A simple queue implementation used by the server to track the player's moves.

#### Usage

Must be ran with **`sudo`** due to the use of **`RAW_SOCKETS`**


#CC = cc -std=c11 -O1 -g -pedantic -Wall -Wextra -Werror -Wfloat-equal -Wshadow -Wpointer-arith -Wunreachable-code -Winit-self -Wno-unreachable-code

CC = cc -std=c11 -O2 -g -pedantic -Wall -Wextra -Werror -Wfloat-equal -Wshadow -Wpointer-arith -Winit-self -Wno-unused-function -Wno-unused-parameter -Wno-variadic-macros


all : client server

client: client.c
	$(CC) -o client -lpthread client.c socket.c dplpmtud.c dplpmtud_udp.c dplpmtud_freebsd.c

server: server.c
	$(CC) -o server -lpthread server.c socket.c dplpmtud.c dplpmtud_udp.c dplpmtud_freebsd.c

clean:
	rm client
	rm server

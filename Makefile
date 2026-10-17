CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS_PTHREAD = -lpthread
LDFLAGS_CRYPTO = -lpthread -lssl -lcrypto

all: client v1 v2 v3 v4

client: client.c
	$(CC) $(CFLAGS) client.c -o client

v1: v1_echo_server/serverEcho.c
	$(CC) $(CFLAGS) v1_echo_server/serverEcho.c -o v1_echo_server/server

v2: v2_multithreaded/serverMulti.c
	$(CC) $(CFLAGS) v2_multithreaded/serverMulti.c -o v2_multithreaded/server $(LDFLAGS_PTHREAD)

v3: v3_command_parser/serverCom.c v3_command_parser/serverComList.c
	$(CC) $(CFLAGS) v3_command_parser/serverCom.c -o v3_command_parser/server_com $(LDFLAGS_PTHREAD)
	$(CC) $(CFLAGS) v3_command_parser/serverComList.c -o v3_command_parser/server_com_list $(LDFLAGS_PTHREAD)

v4: v4_encrypted_chat/serverMess.c v4_encrypted_chat/client.c
	$(CC) $(CFLAGS) -I v4_encrypted_chat v4_encrypted_chat/serverMess.c -o v4_encrypted_chat/server_enc $(LDFLAGS_CRYPTO)
	$(CC) $(CFLAGS) -I v4_encrypted_chat v4_encrypted_chat/client.c -o v4_encrypted_chat/client_enc -lssl -lcrypto

clean:
	rm -f client
	rm -f v1_echo_server/server
	rm -f v2_multithreaded/server
	rm -f v3_command_parser/server_com v3_command_parser/server_com_list
	rm -f v4_encrypted_chat/server_enc v4_encrypted_chat/client_enc

.PHONY: all clean v1 v2 v3 v4


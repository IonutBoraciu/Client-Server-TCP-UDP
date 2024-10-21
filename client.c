// Boraciu Ionut Sorin 325CA
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

// functie care calculeaza modulul unui numar
int makePositive(int num) {
	if (num < 0) {
			return -num;
	} else {
			return num;
	}
}

// se ocupa de afisarea mesajelor corecte
// odata ce server-ul a trimis pachet-ul cu
// datele procesate
void printCorrectMessage(int sockfd, int rc) {
	struct udpMessage message;
	struct completeMessage recvSub;
	char s[1560];

	rc = recv_all(sockfd,&recvSub,sizeof(struct completeMessage));
	DIE(rc < 0, "recv_all FAILED");

	if (recvSub.type == 3) {
		printf("%s:%d - %s - STRING - ", recvSub.ip, recvSub.port, recvSub.topic);
		printf("%s\n",recvSub.mesaj);
	} else if (recvSub.type == 0) {
		printf("%s:%d - %s - INT - ", recvSub.ip, recvSub.port, recvSub.topic);
		printf("%d\n",recvSub.val);
	} else if (recvSub.type == 1) {
		printf("%s:%d - %s - SHORT_REAL - ", recvSub.ip, recvSub.port, recvSub.topic);
		printf("%.02f\n",recvSub.val_short_poz);
	} else if (recvSub.type == 2) {;
		printf("%s:%d - %s - FLOAT - ", recvSub.ip, recvSub.port, recvSub.topic);
		printf("%.*f\n", recvSub.val, recvSub.big_float);
	} else {
		if (strcmp(recvSub.topic, "stop") == 0) {
			close(sockfd);
			exit(0);
		} else {
			// tratez cazul in care cumva server-ul
			// a trimis un pachet care nu a fost inteles
			// de catre client
			printf("Received garbage\n");
		}
	}
}

void subscribe(int sockfd, struct chat_packet sent_packet) {
	// trimite datele primite de la user catre server
	// ( urmand ca acestea sa fie verificate de catre server)
	send_all(sockfd, &sent_packet, sizeof(sent_packet));
	DIE(send_all < 0, "send_all FAILED");
	char message[51];
	strcpy(message, sent_packet.message + 10);
	if (message[strlen(message) - 1] == '\n') {
		message[strlen(message) - 1] = '\0';
	}
	char s[10];
	int rc = recv(sockfd, &s, 11, 0);
	DIE(rc < 0, "recv FAILED");
	if (strcmp(s,"ok") == 0) {
		printf("Subscribed to topic %s\n",message);
	} else {
		// daca server-ul a primit un topic invalid ( gol sau format doar din spatii)
		// atunci va trimite un mesaj "not ok"
		printf("Usage: subscribe <TOPIC>\n");
	}
}

void unsubscribe(int sockfd, struct chat_packet sent_packet) {
	send_all(sockfd, &sent_packet, sizeof(sent_packet));
	char message[51];
	strcpy(message, sent_packet.message + 12);
	if (message[strlen(message) - 1] == '\n') {
		message[strlen(message) - 1] = '\0';
	}
	struct completeMessage sendToSub;
	int rc = recv(sockfd, &sendToSub, sizeof(sendToSub), 0);
	DIE(rc < 0, "recv FAILED");
	if (strcmp(sendToSub.topic,"found") == 0) {
		// daca eram abonati la topic-ul la care dorim
		// sa ne dezabonam
		printf("Unsubscribed to topic %s\n", message);
	} else {
		// mesaj de eroare in caz alternativ
		printf("Couldn't find any subscription with the name %s\n", message);
	}
}

void treatInput(char buf[MSG_MAXSIZE + 1], int sockfd, struct chat_packet sent_packet) {
	sent_packet.len = strlen(buf) + 1;
	strcpy(sent_packet.message, buf);
	// trateaza cele 3 mesaje posibile de input
	// sau afiseaza un mesaj care afiseaza comenziile posibile
	// daca utilizatorul nu a ales o comanda disponibila
	if (strncmp(sent_packet.message, "subscribe", 9) == 0) {
		subscribe(sockfd, sent_packet);
	} else if (strncmp(sent_packet.message, "unsubscribe", 11) == 0) {
		unsubscribe(sockfd,sent_packet);
	} else if (strncmp(sent_packet.message, "exit", 4) == 0) {
		close(sockfd);
		exit(0);
	} else {
		printf("Invalid command\nCommands available:\n- subscribe <TOPIC>\n- unsubscribe <TOPIC>\n- exit\n");
	}
}

void run_client(int sockfd) {
	char buf[MSG_MAXSIZE + 1];
	memset(buf, 0, MSG_MAXSIZE + 1);

	struct chat_packet sent_packet;
	struct pollfd poll_fds[2];

	poll_fds[0].fd = sockfd; 
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = STDIN_FILENO;
	poll_fds[1].events = POLLIN;
	
	while (1) {
		int rc = poll(poll_fds, 2, -1);
		DIE(rc < 0, "poll");
		// se face din o multiplexare, astfel incat
		// sa se aleaga mereu socket-ul activ
		if (poll_fds[0].revents & POLLIN) {
			printCorrectMessage(sockfd,rc);
		}
		if (poll_fds[1].revents & POLLIN) {
			if (!fgets(buf, sizeof(buf), stdin)) {
				break;
			}
			treatInput(buf,sockfd,sent_packet);
		}
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	// ne asiguram ca utilizatorul a rulat corect client-ul
	if (argc != 4) {
		printf("\n Usage: %s<id_client> <ip> <port>\n", argv[0]);
		return 1;
	}
	
	// verific ca port-ul primit este valid
	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");
	
	// creez un socket tcp
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	// ma conectez la server
	rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	int lenId = strlen(argv[1]);
	DIE(rc < 0, "connect");
	if (send(sockfd,argv[1],lenId,0) < 0) {
		printf("Coudlnt send client id\n");
		exit(0);
	}
	run_client(sockfd);
	return 0;
}

// Boraciu Ionut Sorin 325CA
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32
#define INT 0
#define SHORT_FLOAT 1
#define FLOAT 2
#define STRING 3
// Structura de date folosita pentru a tine minte
// pentru fiecare id, socket-ul asociat momentan
// lista de topic-uri la care e abonat, si numarul de elemente
// din lista de topic-uri
struct data {
	int position;
	char subed [50][50];
	int socket_client;
	char id[20];
};

// sparge propozitii de forma /home/Desktop intr-o liste de siruri de caractere
// de forma 
// home
// Desktop
void create_word_list(char *topic, int *val, char word_list[10][50]) {
	char destroy[50];
	strcpy(destroy, topic);
	int poz = 0;
	char *p = strtok(destroy, "/");
	while (p != NULL) {
		strcpy(word_list[poz],p);
		poz++;
		p = strtok(NULL, "/");
	}
	*val = poz;
}

// pregateste structura de date completeMessage care contine
// toate campurile procesate pentru client
// pentru a fi gata sa le afiseze direct
void sendToClient (struct udpMessage mess, int val, uint16_t real_poz, int number,
	int putere, int precision, int udpsocket, struct sockaddr_in udp_client_addr, struct data mySubs) {
	struct completeMessage sendToSub;
	if (mess.type == 0) {
		sendToSub.val = val;
	} else if (mess.type == 1) {
		sendToSub.val_short_poz = (float)real_poz/100;
	} else if (mess.type == 2) {
		sendToSub.val = precision;
		sendToSub.big_float = (float)number/putere;
	}
	strcpy(sendToSub.topic, mess.topic);
	sendToSub.type = mess.type;
	strcpy(sendToSub.mesaj, mess.message);
	strcpy(sendToSub.ip, inet_ntoa(udp_client_addr.sin_addr));
	sendToSub.port = udpsocket;
	DIE(send_all(mySubs.socket_client, &sendToSub, sizeof(struct completeMessage)) < 0, "sendin to client failed");
}

// Functia care verifica wildcard-urile pe datele primite prin UDP
int checkMatching (char subed[50], char word_list[10][50], int poz) {
	char word_list2[10][50];
	int poz_2 = 0;
	int ok = 1;
	int poz_1 = 0;
	// Functia va transforma sirile retinute de fiecare client ALIVE
	// in liste de cuvinte separate prin /
	create_word_list(subed, &poz_2, word_list2);
	// apo compara pe rand element cu element
	for (int l = 0; l < poz_2 && ok; l++) {
		// in cazul in care elementul curent nu este NICI * NICI + si difera comparatia dintre cele 2 elemente
		// inseamna ca wildcard-ul NU face match
		if (strcmp(word_list2[l], word_list[poz_1]) != 0  && strcmp(word_list2[l], "*") != 0 && strcmp(word_list2[l], "+")) {
			ok = 0;
			break;
		} else if (strcmp(word_list2[l], word_list[poz_1]) != 0 && strcmp(word_list2[l], "*") == 0) {
			// in cazul in care am gasit un *, daca se afla la sfarsitul cuvantului
			// inseamna ca avem un match instant, si oprim cautarea
			if (l + 1 >= poz_2) {
				poz_1 = poz;
				break;
			} else {
				// in caz negativ, trec prin elementele din word_list pana cand gasesc
				// urmatorul cuvant care se afla dupa * din word_list2
				while (strcmp(word_list[poz_1], word_list2[l + 1]) != 0) {
					if(poz_1 < poz) {
						poz_1++;
					} else {
						ok = 0;
						break;
					}
				}
			}
		} else {
			poz_1 ++;
		}
	}
	// daca nu am terminat de parcurs word_list, inseamna ca am 
	// ramas cu bucati care nu au facut match, deci nu se potrivesc
	if(poz_1 < poz) {
		ok = 0;
	}
	return ok;
}

// trimite un mesaj catre subscriber, urmand sa fie interceptat si tratat de catre acesta
void sendQuickMessage(char message[50], int sock_id, char packet[100], int type) {
	struct completeMessage sendToSub;
	strcpy(sendToSub.topic, message);
	if (type == 6)
		strcpy(sendToSub.ip, packet + 12);
	sendToSub.type = type;

	DIE(send(sock_id, &sendToSub, sizeof(sendToSub), 0) < 0, "quick message failed");
}

// adauga datele unui abonat nou, mai exact id-ul client-ului, socket-ul, si topicul
// si se asigura ca primeste un mesaj VALID ( care NU este gol si care nu are un topic format doar
// din spatii)
void addSendSub (char message[50], struct data *mySub, char id_client[50], int socket) {
	if (message[strlen(message) - 1] ='\n') {
		message[strlen(message) - 1] = '\0';
	}
	char s[10];
	if(strlen(message) >= 11 && strchr(message+10,' ') == NULL) {
		strcpy((*mySub).id, id_client);
		(*mySub).socket_client = socket;
		strcpy((*mySub).subed[(*mySub).position], message + 10);
		(*mySub).position++;
		strcpy(s,"ok");
	} else {
		strcpy(s,"not ok");
	}

	DIE(send(socket,&s,strlen(s)+1,0) < 0, "addSendSub failed to send");
}

// removeSendSub cauta in baza de date a topic-urilor, daca exista
// topicul pe care dorim sa il eliminam de la client-ul care a trimis mesajul
// in cazul in care nu exista, se trimite un mesaj "not found" care client
// care il va interpreta corespunzator
void removeSendSub(int num_sockets, int listenfd, int udpsocket, char message[50],
		struct pollfd *poll_fds, int socket, struct data *mySubs, char id[50]) {
	if(message[strlen(message) - 1] = '\n') {
		message[strlen(message) - 1] = '\0';
	}
	int check_if_removed = 0;
	for (int j = 0; j < num_sockets; j++) {
		if (poll_fds[j].fd != listenfd && poll_fds[j].fd != udpsocket) {
			if (strcmp(mySubs[j].id, id) == 0) {
				for (int k = 0; k < mySubs[j].position; k++) {
					if (strcmp(mySubs[j].subed[k], message + 12) == 0) {
						for (int p = k; p<mySubs[j].position; p++) {
							strcpy(mySubs[j].subed[p], mySubs[j].subed[p + 1]);
						}
						check_if_removed = 1;
						k--;
						mySubs[j].position--;
					}
				}
			}
		}
	}
	if (check_if_removed == 0) {
		sendQuickMessage("not found", socket, message, 6);
	} else {
		sendQuickMessage("found", socket, message, 6);
	}
}

// interpreteza valorile primite ca si caractere in datele
// cerute
void computeNumbers(struct udpMessage mess, int *val, char s[1560],
		uint16_t *real_poz, int *number, int *putere, int *precision) {
	if (mess.type == INT) {
		// copiaza urmatorii 4 bytes in val
		// ii transforma in host order
		// si verifica semnul
		memcpy(val,mess.message + 1, sizeof(*val));
		DIE(val == NULL, "memcpy failed for int");
		*val = ntohl(*val);
		if ((int)s[51] == 1) {
			*val = -*val;
		}
	} else if (mess.type == SHORT_FLOAT) {
		memcpy(real_poz,mess.message, sizeof(*real_poz));
		DIE(real_poz == NULL, "memcpy failed for real_poz");
		*real_poz = ntohs(*real_poz);
	} else if (mess.type == FLOAT) {
		// primul byte este de semn, urmatorii 4 vor fi
		// numarul in int32 pe care il cautam
		// si urmatorul byte va fi puterea la care trebuie
		// sa impartim numarul pentru a obtine valoarea reala trimisa catre
		// server
		memcpy(number,mess.message+1, sizeof(*number));
		DIE(number == NULL, "memcpy failed for number");
		*number = ntohl(*number);
		if ((int)s[51] == 1) {
			*number = -(*number);
		}
		for (int i = 0;i < (int)s[56]; i++) {
			*putere = (*putere)*10;
		}
		*precision = (int)s[56];
	}
}

void stopServer(int num_sockets, struct pollfd *poll_fds) {
	char buf[50];
	if (!fgets(buf, sizeof(buf), stdin)) {
		exit(0);
	}
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';
	if (strcmp(buf, "exit") == 0) {
		// trimite un mesaj catre toti abonatii
		// pentru a se inchide
		for (int i = 3; i < num_sockets; i++) {
			sendQuickMessage("stop", poll_fds[i].fd, "ign", 5);
		}
		// apoi isi inchide toti sockets deschisi de catre el
		for(int i = 0; i < num_sockets; i++) {
			DIE(close(poll_fds[i].fd) < 0, "close failed what now?");
		}
		// elibereaza structura de date folosita pentru sockets
		free(poll_fds);
		exit(0);
	} else {
		printf("Invalid command\nThe only command that the server can understand is:\nexit\n");
	}
}

void run_chat_multi_server(int tcpsocket, int udpsocket) {
	int capacity_poll = 32, num_sockets = 3, rc, capacity_id = 32, total_clients = 0, capacity = 32;
	// variabila folosita pentru multiplexarea evenimentelor
	struct pollfd *poll_fds = malloc(capacity_poll * sizeof(struct pollfd));

	// retine id-ul fiecarui client in functie de socket-ul acestuia
	char (*id_clients)[20] = malloc(capacity_id * sizeof(*id_clients));
	DIE(id_clients == NULL, "malloc ids FAILED");

	struct data *mySubs = malloc(sizeof(struct data)*capacity);
	DIE(mySubs == NULL, "malloc subs FAILED");

	for (int i = 0; i < 32; i++) {
		mySubs[i].position = 0;
	}
	struct chat_packet received_packet;
	rc = listen(tcpsocket, MAX_CONNECTIONS);
	DIE(rc < 0, "listen");
	// initializez server-ul cu cele 3 tipuri de conexiuni
	// tcpsocket, udpsocket, stdin
	poll_fds[0].fd = tcpsocket;
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = udpsocket;
	poll_fds[1].events = POLLIN;

	poll_fds[2].fd = STDIN_FILENO;
	poll_fds[2].events = POLLIN;

	while (1) {
		// astept sa se intample un evimenet pe unul dintre sockets
		rc = poll(poll_fds, num_sockets, -1);
		DIE(rc < 0, "poll");
		for (int i = 0; i < num_sockets; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == tcpsocket) {
					// pe socket-ul tcp voi incercepta incercarile de conectare
					// ale unui noi abonat catre server
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					const int newsockfd = accept(tcpsocket, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");
					char *id_client = malloc(20);
					DIE(id_client == NULL, "malloc FAILED");
					if (recv(newsockfd, id_client, 20, 0) < 0) {
						printf("No id found? Maybe missuse?\n");
						exit(0);
					}
					int stop2 = 0;
					// verific in vectorul de siruri de caractere id_clients DACA am deja
					// clientul cu id-ul nou conectat
					// caz in care voi da reject la conexiune
					// si voi trimite un semnal de oprire catre client
					for (int j = 0;j < capacity_id; j++) {
						if (strcmp(id_client, id_clients[j]) == 0 && strlen(id_clients[j]) !=0) {
							printf("Client %s already connected.\n", id_client);
							sendQuickMessage("stop", newsockfd, "ign", 5);
							stop2 = 1;
						}
		  			}
					// inchid conexiunea cu socket-ul daca deja aveam
					// un client cu acel id conectat
					if (stop2 == 1) {
						close(newsockfd);
					}

					if (stop2 == 0) {
						// daca id-ul adaugat este unic pe server
						// il adaug la liste curenta
						poll_fds[num_sockets].fd = newsockfd;
						poll_fds[num_sockets].events = POLLIN;
						num_sockets++;
						if (num_sockets >= capacity_poll) {
							// maresc capacitatea la poll_fds daca este necesar
							poll_fds = realloc(poll_fds,sizeof(struct pollfd) * (capacity_poll * 2));
							capacity_poll *= 2;
							DIE(poll_fds == NULL, "realloc FAILED");
						}
						printf("New client %s connected from %s:%d.\n", id_client,
						inet_ntoa(cli_addr.sin_addr), newsockfd);
						int stop = 0;
						// verific daca client-ul s-a reconectat ( a mai fost conectat)
						// daca da, voi actualiza socket-ul din structura care retine
						// in functie de id, topic-urile la care este abonat
						for (int j = 0;j < num_sockets && !stop; j++) {
							if (poll_fds[j].fd != tcpsocket && poll_fds[j].fd != udpsocket) {
						 		for (int k = 0; k < mySubs[j].position && !stop; k++) {
						 			if (strcmp(id_client, mySubs[j].id) == 0) {
										mySubs[j].socket_client = newsockfd;
										stop = 1;
									}
								}
							}
						}
						total_clients++;
						// verific daca este nevoie de redimensionari
						// in caz afirmativ vor fi facute realloc-urile
						// si verificate
						if (total_clients >= capacity) {
							int old_capacity = capacity;
							capacity = capacity * 2;
       						mySubs = realloc(mySubs, sizeof(struct data) * capacity);
							DIE(mySubs == NULL, "realloc FAILED");
        					for (int j = old_capacity; j < capacity; j++) {
           					mySubs[j].position = 0;
        					}
    					}
						if (newsockfd >= capacity_id) {
							capacity_id *= 2;
							id_clients = realloc(id_clients, capacity_id * sizeof(*id_clients));
							DIE(id_clients == NULL, "realloc FAILED");
						}
						strcpy(id_clients[newsockfd], id_client);
						free(id_client);
						break;
					}
				} else if (poll_fds[i].fd == udpsocket) {
					// Daca ma aflu pe un socket udp inseamna
					// ca trebuie sa verific daca vreunul dintre clienti
					// este abonat la vreun topic primit prin UDP
					struct sockaddr_in udp_client_addr;
					socklen_t udp_client_len = sizeof(udp_client_addr);
					struct udpMessage mess;
					char *s = calloc(1560, 1);
					DIE(s == NULL, "calloc FAILED");
					int rc = recvfrom(udpsocket, s, 1560, 0,
						(struct sockaddr *)&udp_client_addr, &udp_client_len);
					if(rc < 0) {
						DIE(rc < 0, "recfrom FAILED");
					}

					memcpy(mess.topic, s, 50);
					memcpy(&mess.type, s + 50, 1);
					memcpy(mess.message, s + 51, 1560);
					uint16_t real_poz;
					int number = 0, putere = 1, precision, val;
					// se calculeaza fiecare numar corespunzator ( functie explicata mai sus)
					computeNumbers(mess, &val, s, &real_poz, &number, &putere, &precision);
					int poz = 0;
					char word_list[10][50];
					create_word_list(mess.topic, &poz,word_list);
					for (int j = 0; j < num_sockets; j++) {
			  			if (poll_fds[j].fd != tcpsocket && poll_fds[j].fd != udpsocket) {
				  			for (int k = 0; k < mySubs[j].position; k++) {
								// verific daca trebuie sa fac pattern matching pentru a afla
								// daca ce am primit de la udp trebuie trimis catre vreun client
								// in caz afirmativ, voi trimite mesajul cu datele deja procesate catre client
								// care trebuie sa se ocupe doar de afisarea lor
					 			if (strchr (mySubs[j].subed[k], '+') != NULL || strchr(mySubs[j].subed[k], '*') != NULL) {
						  			int ok = checkMatching(mySubs[j].subed[k], word_list,poz);
									if(ok) {
						  				sendToClient(mess, val, real_poz, number, putere, precision, udpsocket, udp_client_addr, mySubs[j]);
						  				break;
									}
								} else if(strncmp(mySubs[j].subed[k],mess.topic,strlen(mess.topic)) == 0) {
										sendToClient(mess, val, real_poz, number, putere, precision, udpsocket, udp_client_addr, mySubs[j]);
					  			}

							}
			  			}
					}
				} else if(poll_fds[i].fd != STDIN_FILENO) {
					// verific daca clientul s-a deconectat sau
					// daca am primit un input de subscribe sau unsubscribe de la CLIENT
					int rc = recv(poll_fds[i].fd, &received_packet, sizeof(received_packet), 0);
					DIE(rc < 0, "recv");
					if (rc == 0) {
						// verific daca client-ul s-a deconectat, in caz afirmativ
						// il scot din poll si scad dimensiunea socket-urilor active
						printf("Client %s disconnected.\n", id_clients[poll_fds[i].fd]);
						strcpy(id_clients[poll_fds[i].fd], "");
						for (int j = i; j < num_sockets - 1; j++) {
 							poll_fds[j] = poll_fds[j + 1];
						}
						num_sockets--;
		  			} else {
						if (strncmp(received_packet.message, "subscribe", 9) == 0) {
							addSendSub(received_packet.message, &mySubs[i], id_clients[poll_fds[i].fd], poll_fds[i].fd);
						} else if (strncmp(received_packet.message,"unsubscribe",11) == 0) {
							removeSendSub(num_sockets, tcpsocket, udpsocket, received_packet.message, poll_fds, poll_fds[i].fd, mySubs, id_clients[poll_fds[i].fd]);
						} else {
							printf("Invalid command\n- Commands available: \n- subscribe <TOPIC>\n- unsubscribe <TOPIC>\n");
						}
		  			}
				} else {
					free(mySubs);
					free(id_clients);
					stopServer(num_sockets, poll_fds);
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc != 2) {
		printf("\n Usage: %s <ip> <port>\n", argv[0]);
		return 1;
	}
	uint16_t port;
	int rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");
	// Deschid 2 sockets, unul tcp, unul udp
	const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	const int udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
	// ma asigur ca ambii sunt deschisi
	DIE(listenfd < 0, "socket");
	DIE(udpsocket < 0, "udpsocket");

	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);
	const int enable = 1;
	int flag = 1;
	if (setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY,&enable, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");
	if (setsockopt(udpsocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");
	int rc2 = bind(udpsocket,(const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc2 < 0, "bind");
	run_chat_multi_server(listenfd,udpsocket);

	return 0;
}

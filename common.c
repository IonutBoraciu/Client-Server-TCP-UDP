#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>

/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len)
{
    int rc;
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;

    while (bytes_remaining)
    {
        rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        bytes_received += rc;
        bytes_remaining -= rc;
    }

    return bytes_received;
}

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

int send_all(int sockfd, void *buffer, size_t len)
{
    int rc;
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;

    while(bytes_remaining)
    {
        rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        bytes_sent += rc;
        bytes_remaining -= rc;
    }

    return bytes_sent;
}

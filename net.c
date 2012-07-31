#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>

#include"common.h"
#include"rain.h"
#include"log.h"

static int sock_create() {

    int sock, i = 1;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        logger(ERROR, "create error");
        return RAIN_ERROR;
    }

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1) {
        logger(ERROR, "set socket reuseadd error");
        return RAIN_ERROR;
    }

    return sock;
}

static int sock_listen(int sock, struct sockaddr *sa, int len) {

    if (bind(sock, sa, len) == -1) {
        logger(ERROR, "bind socket error");
        return RAIN_ERROR;
    }

    if (listen(sock, 511) == -1) {
        logger(ERROR, "listen error");
        return RAIN_ERROR;
    }

    return RAIN_OK;
}

int start_server() {

    struct sockaddr_in addr_in;
    int sock, sock_len = sizeof(addr_in);

    memset(&addr_in, 0, sock_len);
    addr_in.sin_family = AF_INET;
    if (-1 == inet_pton(AF_INET, rain_srv.addr, &addr_in.sin_addr.s_addr)) {
        logger(WARNING, "bind addr error, now use INADDR_ANY");
        addr_in.sin_addr.s_addr = INADDR_ANY;
    }

    addr_in.sin_port = htons(rain_srv.port);

    sock = sock_create();
    if ((sock_listen(sock, (struct sockaddr *) & addr_in , sock_len)) == RAIN_ERROR) {
        return RAIN_ERROR;
    }

    rain_srv.sock_fd = sock;
    return RAIN_OK;
}

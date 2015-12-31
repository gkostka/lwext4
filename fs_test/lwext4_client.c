/*
 * Copyright (c) 2014 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
static int inet_pton(int af, const char *src, void *dst);

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#endif

static int winsock_init(void);
static void winsock_fini(void);

/**@brief   Default server addres.*/
static char *server_addr = "127.0.0.1";

/**@brief   Default connection port.*/
static int connection_port = 1234;

/**@brief   Call op*/
static char *op_code;

static const char *usage = "                                    \n\
Welcome in lwext4_client.                                       \n\
Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
Usage:                                                          \n\
    --call (-c) - call opt                                      \n\
    --port (-p) - server port                                   \n\
    --addr (-a) - server ip address                             \n\
\n";

static int client_connect(void)
{
	int fd = 0;
	struct sockaddr_in serv_addr;

	if (winsock_init() < 0) {
		printf("winsock_init error\n");
		exit(-1);
	}

	memset(&serv_addr, '0', sizeof(serv_addr));
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("socket() error: %s\n", strerror(errno));
		exit(-1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(connection_port);

	if (!inet_pton(AF_INET, server_addr, &serv_addr.sin_addr)) {
		printf("inet_pton() error\n");
		exit(-1);
	}

	if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
		printf("connect() error: %s\n", strerror(errno));
		exit(-1);
	}

	return fd;
}

static bool parse_opt(int argc, char **argv)
{
	int option_index = 0;
	int c;

	static struct option long_options[] = {
			{"call", required_argument, 0, 'c'},
			{"port", required_argument, 0, 'p'},
			{"addr", required_argument, 0, 'a'},
			{"version", no_argument, 0, 'x'},
			{0, 0, 0, 0}};

	while (-1 != (c = getopt_long(argc, argv, "c:p:a:x", long_options,
			&option_index))) {

		switch (c) {
		case 'a':
			server_addr = optarg;
			break;
		case 'p':
			connection_port = atoi(optarg);
			break;
		case 'c':
			op_code = optarg;
			break;
		case 'x':
			puts(VERSION);
			exit(0);
			break;
		default:
			printf("%s", usage);
			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[])
{
	int sockfd;
	int n;
	int rc;
	char recvBuff[1024];

	if (!parse_opt(argc, argv))
		return -1;

	sockfd = client_connect();

	n = send(sockfd, op_code, strlen(op_code), 0);
	if (n < 0) {
		printf("\tWrite error: %s fd = %d\n", strerror(errno), sockfd);
		return -1;
	}

	n = recv(sockfd, (void *)&rc, sizeof(rc), 0);
	if (n < 0) {
		printf("\tWrite error: %s fd = %d\n", strerror(errno), sockfd);
		return -1;
	}

	printf("rc: %d %s\n", rc, strerror(rc));
	if (rc)
		printf("\t%s\n", op_code);

	winsock_fini();
	return rc;
}

static int winsock_init(void)
{
#if WIN32
	int rc;
	static WSADATA wsaData;
	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc != 0) {
		return -1;
	}
#endif
	return 0;
}

static void winsock_fini(void)
{
#if WIN32
	WSACleanup();
#endif
}

#if WIN32
static int inet_pton(int af, const char *src, void *dst)
{
	struct sockaddr_storage ss;
	int size = sizeof(ss);
	char src_copy[INET6_ADDRSTRLEN + 1];

	ZeroMemory(&ss, sizeof(ss));
	/* stupid non-const API */
	strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
	src_copy[INET6_ADDRSTRLEN] = 0;

	if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss,
			&size) == 0) {
		switch (af) {
		case AF_INET:
			*(struct in_addr *)dst =
					((struct sockaddr_in *)&ss)->sin_addr;
			return 1;
		case AF_INET6:
			*(struct in6_addr *)dst =
					((struct sockaddr_in6 *)&ss)->sin6_addr;
			return 1;
		}
	}
	return 0;
}
#endif

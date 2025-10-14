#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>


#include "dptm_tls_mon.h"
#include "mon_print.h"

#define DEBUG_LOG
#ifdef DEBUG_LOG
#define debug_log_printf printf
#else
#define debug_log_printf(str, ...) do{}while(0);
#endif


#define BUFF_LEN (32*1024)
#define BLOCK_LEN 4*1024

int 
mon_client_task(char* addr, int port)
{
    int ret = -1;

    int sockfd;
    int b_run;
    int pos;
    int len;
    int pkt_len;
    int data_len;

    struct sockaddr_in server_addr;
    uint8_t *buf = NULL;

    buf = (uint8_t*)malloc(BUFF_LEN);
    if(buf == NULL)
        return -1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) {
        printf("MON> create socket failed.\n");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(addr); 
    server_addr.sin_port = htons(port);
    printf("connect %s:%d...", addr, port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("fail.\n");
        return -1;
    }
    printf("ok.\n");

    b_run = 1;
    while(b_run > 0) {
        usleep(1);

        pos = 0;
        ret = read(sockfd, buf, 4);
        debug_log_printf("read head len=%d\n", ret);
        if(ret < 4) {
            printf("recv: head error....ret=%d\n", ret);
            break;
        }
        pkt_len = ((int)buf[2] << 8) | buf[3];

        debug_log_printf("read head cmd=%d seq=%d pkt_len=%d\n", buf[0], buf[1], pkt_len);

        data_len = pkt_len;
        pos = 4;
        while(b_run > 0 && data_len > 0) {
            len = BLOCK_LEN;
            if(data_len < BLOCK_LEN) {
                len = data_len;
            }

            ret = read(sockfd, buf + pos, len);
            debug_log_printf("read data pos=%d (%d) ret=%d\n", pos, pkt_len, ret);
            if(ret < 0) {
                b_run = 0;
                printf("recv: data error....len=%d (ret=%d)\n", len, ret);
                break;
            }
            data_len -= ret;
            pos += ret;
        }

        // recv done
        //mon_print_packet(buf, pos);

        pos = 0;
    }

    if(sockfd >=0) {
        close(sockfd);
    }

    if(buf != NULL)
        free(buf);

    return 0;
}

#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 14445

#define DEBUG_ARGS for (int i = 1; i < argc; i++) { printf("argc%d=%s\n", i, argv[i]); } printf("\n");
#define USAGE_MESG "Usage: %s [target_addr] [port]\n"

#define BUF_LEN 1024

int
main(int argc, char* argv[])
{
    char *addr = DEFAULT_ADDR;
    int port = DEFAULT_PORT;

    printf(USAGE_MESG, argv[0]);
    DEBUG_ARGS;

    if (argc > 1) {
        addr = argv[1];
    }
    if (argc > 2) {
        port = atol(argv[2]);
    }

    mon_client_task(addr, port);

    return 0;
}

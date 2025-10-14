#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "dptm_tls_mon.h"
#include "mon_print.h"

//#define DISABLE_SEND_PACKET
//#define DISABLE_TIME_LIVE

//#define DEBUG_LOG
#ifdef DEBUG_LOG
#define debug_log_printf printf
#else
#define debug_log_printf(str, ...) do{}while(0);
#endif

static int64_t 
get_utc(void)
{
    int64_t  cur_time = 0;
#if defined(__linux__)
    struct  timeval  tv;
    gettimeofday(&tv, NULL);
    cur_time  = tv.tv_sec;
    cur_time *= 1000;
    cur_time += tv.tv_usec / 1000;
#endif
    return cur_time;
}

#define BUFF_LEN (64*1024)
#define BLOCK_LEN 4*1024

#define MON_SRV_CLI_MAX 5
#define TS_TIME_INVALID 0xFFFFFFFFU

#define MON_SESS_BUF_BUCKET_SIZE (16*1024)

int 
mon_server_task(char* filename, int port)
{
    int ret = -1;

    int b_run;
    int b_run_client;
    int pos;
    int len;
    int pkt_len;
    int data_len;

    uint32_t ts;
    uint32_t ts_first;

    int64_t cur;
    int64_t cur_timeout;
    int64_t ref_time;

    int sockfd = -1;
    struct sockaddr_in server_addr;
    uint8_t *buf = NULL;

    struct sockaddr_in client_addr;
    socklen_t socklen = sizeof(client_addr);
    int client_fd = -1;

    FILE* fp = NULL;

    buf = (uint8_t*)malloc(BUFF_LEN);
    if(buf == NULL)
        return -1;

    fp = fopen(filename, "rb");
    printf("MON> open record file= %s", filename);
    if(fp == NULL) {
        printf("...fail.\n");
        return -1;
    }
    printf("...ok.\n");
    
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) {
        printf("MON> create socket failed.\n");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    {
        struct timeval timeout;
        timeout.tv_sec = 3;  // 3sec
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            printf("MON> ERROR, setsockopt SO_RCVTIMEO=%ld\n", timeout.tv_sec);
            return -1;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            printf("MON> ERROR, setsockopt SO_SNDTIMEO=%ld\n", timeout.tv_sec);
            return -1;
        }    
    }

    printf("MON> Listen port=%d\n", port);
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("MON> ERROR, bind port=%d\n", port);
        return -1;
    }

    if(listen(sockfd, MON_SRV_CLI_MAX) < 0) {
        printf("MON> ERROR, listen ret=%d\n", port);
        return -1;
    }

    b_run = 1;
    while(b_run > 0) {

        b_run_client = 1;

#ifndef DISABLE_SEND_PACKET
        client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &socklen);
        if(client_fd < 0) {
            printf("MON> wait, accept\n");
            continue;
        } else {
            printf("connected... %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
#endif
        usleep(100000);

        fseek(fp, 0, SEEK_SET);

        ref_time = -1;
        ts_first = TS_TIME_INVALID;

        cur_timeout = 0;
        while(b_run > 0) {

            // READ: dump packet #########################
            // read head
            pos = 0;
            ret = fread(buf, 1, 4, fp);
            debug_log_printf("file> read head_len(4) ret=%d\n", ret);
            if(ret < 4) {
                printf("reload dump position to the beginning...\n");
                fseek(fp, 0, SEEK_SET);
                usleep(1000000);
                continue;
            }
            pkt_len = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 0);
            if(pkt_len >= MON_SESS_BUF_BUCKET_SIZE) {
                printf("MON> error, packet len=%d (must < %d)\n", pkt_len, MON_SESS_BUF_BUCKET_SIZE);
                break;
            }

            // read payload
            pos = 4;
            ret = fread(buf + pos, 1, pkt_len, fp);
            debug_log_printf("file> read payload(%d) ret=%d\n", pkt_len, ret);
            if(ret < pkt_len) {
                printf("reload dump position to the beginning...\n");
                fseek(fp, 0, SEEK_SET);
                usleep(1000000);
                break;
            }

            ts = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | ((uint32_t)buf[7] << 0);
            debug_log_printf("file> ts=%7.3f (%lu)\n", (float)ts/1000, ts);
            if(ts == 0) {
                continue;
            }

            if(ts_first == TS_TIME_INVALID) {
                ts_first = ts;
            }
            ts = ts - ts_first;
            debug_log_printf("file> ts2=%7.3f (%lu)\n", (float)ts/1000, ts);
            
            // WAIT timestamp #########################
            while(b_run > 0) {
                cur = get_utc();
                if(ref_time < 0) {
                    ref_time = cur;
                }
                cur = cur - ref_time;
                if(cur > ts) {
                    cur_timeout = cur + 500;
                    break;

                } else {
                    if(cur > cur_timeout) {
                        cur_timeout = cur + 500;
                        printf("%7.3f \n",(float)cur / 1000);
#ifdef DISABLE_TIME_LIVE
                        break;
#endif
                    }
                }
                usleep(1000);
            }

            // SEND packet #########################
            printf("%7.3f ",(float)cur / 1000);
            pos = 8;
            pkt_len = pkt_len - 4;
            mon_print_packet(buf+pos, pkt_len);

#ifndef DISABLE_SEND_PACKET
            data_len = pkt_len;
            while(b_run > 0 && b_run_client > 0 && data_len > 0) {
                len = BLOCK_LEN;
                if(data_len < BLOCK_LEN) {
                    len = data_len;
                }
                ret = send(client_fd, buf + pos, len, MSG_NOSIGNAL);
                debug_log_printf("write data pos=%d (%d) ret=%d\n", pos, pkt_len, ret);
                if(ret < 0) {
                    b_run_client = 0;
                    printf("send: data error....len=%d (ret=%d)\n", len, ret);
                    break;
                }
                data_len -= ret;
                pos += ret;
            }
#endif

            if(b_run_client == 0)
                break;
        }

        if(client_fd >= 0) {
            close(client_fd);
            client_fd = -1;
        }
    }

    if(fp != NULL) {
        fclose(fp);
    }

    if(sockfd >=0) {
        close(sockfd);
    }

    if(buf != NULL)
        free(buf);

    return 0;
}

#define DEFAULT_PORT 14445

#define DEFAULT_DUMP_FILE "record_mon.dump"
#define DEBUG_ARGS for (int i = 1; i < argc; i++) { printf("argc%d=%s\n", i, argv[i]); } printf("\n");
#define USAGE_MESG "Usage: %s record_file.dump [port]\n"

#define BUF_LEN 1024

int
main(int argc, char* argv[])
{
    char *filename = DEFAULT_DUMP_FILE;
    int port = DEFAULT_PORT;

    printf(USAGE_MESG, argv[0]);
    DEBUG_ARGS;

    if (argc > 1) {
        filename = argv[1];
    }

    if (argc > 2) {
        port = atol(argv[2]);
    }

    mon_server_task(filename, port);

    return 0;
}

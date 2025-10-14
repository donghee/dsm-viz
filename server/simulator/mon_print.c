#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dptm_tls_mon.h"
#include "mon_print.h"

int
dump_parse_mavlink_packet(uint8_t* data, int len)
{
    int ret = 0;
    int msgid;
    if(len >2 && (data[0] == 0xFD || data[0] == 0xFE) && data[1] > 0) {
        msgid = data[5];
        if(data[0] == 0xFD) {
            msgid = (int)data[7] | ((int)data[8] << 8) | ((int)data[9] << 16);
        }
        printf(" MAV %s", mavlink_msg_id_to_str(msgid));
        ret = 1;
    }
    return ret;
}


int
dump_parse_openvpn_packet(uint8_t* data, int len)
{
    int ret = 0;
    if(len >4 && data[0] == 0x48 && data[1]==0x00 && data[2]==0x00 && data[3]==0x00 ) {
#if 0
        int msgid;
        int keyid;
        int peerid;
        msgid = data[0] >> 3;
        keyid = data[0] & 0x7;
        //peerid = (int)data[1] | ((int)data[2] << 8) | ((int)data[3] << 16);
        if(msgid == 0x9) {
            printf(" OPENVPN P_DATA_V2");
            ret = 1;
        }
#else
        printf(" OPENVPN P_DATA_V2");
        ret = 1;
#endif
    }
    return ret;
}


# define TLS1_2_VERSION                  0x0303
# define TLS1_3_VERSION                  0x0304
# define DTLS1_2_VERSION                 0xFEFD

int
dump_parse_tls_packet(uint8_t* data, int len)
{
    int ret = 0;
    if(len > 4) {
        int ver;
        uint8_t type;
        char* s_ver = NULL;
        char* s_type = NULL;

        type = data[0];
        switch(type) {
        case 20: //SSL3_RT_CHANGE_CIPHER_SPEC
            s_type = "ChangeCipherSpec";
            break;
        case 21: //SSL3_RT_ALERT:
            s_type = "Alert";
            break;
        case 22: //SSL3_RT_HANDSHAKE:
            s_type = "Handshake";
            break;
        case 23: // SSL3_RT_APPLICATION_DATA:
            s_type = "AppData";
            break;
        default:
            break;
        }

        if(s_type == NULL)
            return 0;

        ver = (int)data[1] << 8 | data[2];
        if( ver == TLS1_2_VERSION ) {
            s_ver = "TLS1.2";
        } else if( ver == TLS1_3_VERSION) {
            s_ver = "TLS1.3";
        } else if( ver == DTLS1_2_VERSION) {
            s_ver = "DTLS1.2";
        } else {
            return 0;
        }

        printf(" %s %s", s_ver, s_type);
        ret = 1;
    }
    return ret;
}



static char* g_mon_cmd_str[] = {
    "RCV    ",            // 0
    "SND    ",            // 1
    "RCV-S  ",            // 2
    "SND-S  ",            // 3
    "P-RCV  ",            // 4
    "P-SND  ",            // 5
    "P-RCV-S",            // 6
    "P-SND-S",            // 7
    "PING   ",            // 8
    "RSVED09",            // 9
    "RSVED10",            // 10
    "RSVED11",            // 11
    "RSVED12",            // 12
    "RSVED13",            // 13
    "RSVED14",            // 14
    "RSVED15",            // 15
    "STATE  ",            // 16
};


#define MON_SEQ_INIT_VALUE 0xFFFFFFFFU
uint32_t g_seq_prev = MON_SEQ_INIT_VALUE;
int
mon_print_packet(uint8_t* data, int len)
{
    uint8_t cmd = data[0];
    uint8_t seq = data[1];
    int pkt_len = ((int)data[2] << 8) | data[3];
    int decode_pos = 4;

    if(cmd > DPTM_MON_CMD_STATE || len < 0 || pkt_len < 0) {
        printf("DUMP> head error, cmd=%d, pkt_len=%d\n", cmd, pkt_len);
    }

    if(g_seq_prev == MON_SEQ_INIT_VALUE) {
        g_seq_prev = seq;
    } else {
        g_seq_prev = (g_seq_prev + 1) & 0xFF;
        if(g_seq_prev != seq) {
            int count = (int)seq - g_seq_prev;
            if(count < 0) {
                count += 256;
            }
            printf("DUMP> SEQ mismatch....DROP %d (seq=%d, must=%d)\n", count, seq, g_seq_prev);
            g_seq_prev = seq;
        }
    }

    if(pkt_len > 0) {
        #define IP_AND_UDP_HEAD_LEN 28

        // check PACKET mode
        if((cmd & DPTM_MON_BIT_PACKET) > 0) {
            if(pkt_len > IP_AND_UDP_HEAD_LEN && data[4]  == 0x45 /* IP */ && data[13] == 0x11 /* UDP */) {
                decode_pos = 4 + IP_AND_UDP_HEAD_LEN;
                pkt_len -= IP_AND_UDP_HEAD_LEN;
            }

            libcup_dump_print(g_mon_cmd_str[cmd], data+decode_pos, pkt_len);
            if(dump_parse_openvpn_packet(data+decode_pos, pkt_len)) {
                printf("\n");
                return 1;
            }
        } else {
            libcup_dump_print(g_mon_cmd_str[cmd], data+decode_pos, pkt_len);
            if(dump_parse_tls_packet(data+decode_pos, pkt_len) > 0) {
                printf("\n");
                return 1;
            }
        }

        if(dump_parse_mavlink_packet(data+decode_pos, pkt_len) > 0) {
            printf("\n");
            return 1;
        }
        printf("\n");
    }

    return 0;
}

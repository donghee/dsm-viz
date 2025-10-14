#ifndef __DPTM_TLS_MON_H__
#define __DPTM_TLS_MON_H__

#include <stdint.h>
#define DPTM_MON_PROTO_HEAD_LEN 4
#define DPTM_MON_PROTO_POS_CMD 0
#define DPTM_MON_PROTO_POS_SEQ 1
#define DPTM_MON_PROTO_POS_LEN 2
#define DPTM_MON_PROTO_POS_DATA 4

#define DPTM_MON_BIT_RECV    0
#define DPTM_MON_BIT_SEND    1
#define DPTM_MON_BIT_ENCRYPT 2
#define DPTM_MON_BIT_PACKET  4
#define DPTM_MON_BIT_PING    8

#define DPTM_MON_CMD_RECV          0
#define DPTM_MON_CMD_SEND          1
#define DPTM_MON_CMD_ENC_RECV      2
#define DPTM_MON_CMD_ENC_SEND      3
#define DPTM_MON_CMD_PKT_RECV      4
#define DPTM_MON_CMD_PKT_SEND      5
#define DPTM_MON_CMD_PKT_ENC_RECV  6
#define DPTM_MON_CMD_PKT_ENC_SEND  7
#define DPTM_MON_CMD_PING          8
#define DPTM_MON_CMD_STATE         16
#define DPTM_MON_CMD_TYPE_MAX      DPTM_MON_CMD_STATE

typedef void* dptm_tls_mon_handle;


#ifdef __cplusplus
extern "C" {
#endif

    dptm_tls_mon_handle dptm_tls_mon_open_server(int port);
    int dptm_tls_mon_close(dptm_tls_mon_handle in_handle);
    int dptm_mon_set_default_handle(dptm_tls_mon_handle *handle);

    int dptm_mon_write_packet(dptm_tls_mon_handle *handle, int cmd, uint8_t* data, int len);
    int dptm_mon_write_data(dptm_tls_mon_handle *handle, uint8_t* data, int len);
    int dptm_mon_set_info_ciphersuite(dptm_tls_mon_handle handle, char* tls_ver, char* kem_alg, char* sig_alg, char* ciphersuite);
    int dptm_mon_set_info_connect(dptm_tls_mon_handle handle, char* connect_info);

    dptm_tls_mon_handle dptm_tls_mon_open_client(const char *addr, int port);
    int dptm_tls_mon_close_client(dptm_tls_mon_handle in_handle);

    int dptm_mon_dump(const char* title, unsigned char* data, int len);
    int dptm_mon_packet(int cmd, uint8_t* data, int len);
    int dptm_mon_set_info(char* connect_state, char* tls_ver, char* kem_alg, char* sig_alg, char* ciphersuite);

#ifdef __cplusplus
}
#endif

#endif //__DPTM_TLS_MON_H__

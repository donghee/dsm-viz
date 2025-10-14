#ifndef __MON_COMMON_H__
#define __MON_COMMON_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    int libcup_dump_print(const char* title, unsigned char* data, int len);
    const char* mavlink_msg_id_to_str(int msgid);
    int dump_parse_mavlink_packet(uint8_t* data, int len);
    int dump_parse_openvpn_packet(uint8_t* data, int len);
    int mon_print_packet(uint8_t* data, int len);
    
#ifdef __cplusplus
}
#endif

#endif // __MON_COMMON_H__
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <common/mavlink.h>
#include <mavlink_helpers.h>
#include <mavlink_get_info.h>

using json = nlohmann::json;
using namespace std;

typedef websocketpp::client<websocketpp::config::asio_client> client_t;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

enum ErrorCode {
    SUCCESS = 0,
    ERROR_CONFIG_LOAD = -1,
    ERROR_WEBSOCKET_INIT = -2,
    ERROR_WEBSOCKET_CONNECT = -3,
    ERROR_SOCKET_CREATE = -4,
    ERROR_SOCKET_CONNECT = -5,
    ERROR_RECV_HEADER = -6,
    ERROR_RECV_DATA = -7,
    ERROR_WEBSOCKET_SEND = -8
};

class DSMClient {
private:
    string target_addr;
    int target_port;
    string websocket_server;
    int restart_delay;

    const int BUFF_LEN = 32 * 1024;
    const int BLOCK_LEN = 4 * 1024;

    client_t ws_client;
    websocketpp::connection_hdl ws_hdl;
    thread ws_thread;
    bool connected;

    void cleanupWebSocket() {
        if (connected) {
            ws_client.close(ws_hdl, websocketpp::close::status::normal, "");
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }
    }

    string getMavlinkMessageName(uint32_t msgid) {
        static const map<string, uint32_t> name_to_msgid = MAVLINK_MESSAGE_NAMES;
        static map<uint32_t, string> msgid_to_name;
        if (msgid_to_name.empty()) {
            for (const auto& pair : name_to_msgid) {
                msgid_to_name[pair.second] = pair.first;
            }
        }

        auto it = msgid_to_name.find(msgid);
        if (it != msgid_to_name.end()) {
            return it->second;
        }
        return "UNKNOWN_" + to_string(msgid);
    }

    const json mavlink_get_message_json(const mavlink_message_t* msg) {
        json msg_json;

        const mavlink_message_info_t* msg_info = mavlink_get_message_info(msg);
        if (!msg_info) {
            return msg_json; // Return empty JSON if message info not found
        }
        msg_json["message"] = msg_info->name;
        msg_json["msgid"] = (int)msg->msgid;
        msg_json["sysid"] = msg->sysid;
        msg_json["compid"] = msg->compid;
        msg_json["seq"] = msg->seq;
        msg_json["len"] = msg->len;
        msg_json["checksum"] = (int)msg->checksum;

        for (uint8_t i = 0; i < msg_info->num_fields; i++) {
            const mavlink_field_info_t& field = msg_info->fields[i];

            // Handle different field types, ignore arrays for simplicity
            switch (field.type) {
                case MAVLINK_TYPE_CHAR: {
                    msg_json["payload"][field.name] = string(1, _MAV_RETURN_char(msg, field.wire_offset));
                    break;
                }
                case MAVLINK_TYPE_UINT8_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_uint8_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_INT8_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_int8_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_UINT16_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_uint16_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_INT16_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_int16_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_UINT32_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_uint32_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_INT32_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_int32_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_UINT64_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_uint64_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_INT64_T: {
                    msg_json["payload"][field.name] = _MAV_RETURN_int64_t(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_FLOAT: {
                    msg_json["payload"][field.name] = _MAV_RETURN_float(msg, field.wire_offset);
                    break;
                }
                case MAVLINK_TYPE_DOUBLE: {
                    msg_json["payload"][field.name] = _MAV_RETURN_double(msg, field.wire_offset);
                    break;
                }
                default:
                    // Unsupported type
                    msg_json["payload"][field.name] = nullptr;
                    break;
             }
        }
        return msg_json;
    }


public:
    DSMClient() : connected(false) {
    }

    int initialize() {
        int ret = loadConfig();
        if (ret != SUCCESS) return ret;

        ret = initWebSocket();
        if (ret != SUCCESS) return ret;

        return SUCCESS;
    }

    void deinitialize() {
        cleanupWebSocket();
    }

  int loadConfig() {
    // Read configuration from environment variables
    const char* target_addr_env = getenv("TARGET_ADDR");
    target_addr = target_addr_env ? string(target_addr_env) : "127.0.0.1";

    const char* target_port_env = getenv("TARGET_PORT");
    target_port = target_port_env ? stoi(string(target_port_env)) : 14445;

    const char* websocket_server_env = getenv("WEBSOCKET_SERVER");
    websocket_server = websocket_server_env ? string(websocket_server_env) : "ws://127.0.0.1:8000/ws";

    const char* restart_delay_env = getenv("RESTART_DELAY");
    restart_delay = restart_delay_env ? stoi(string(restart_delay_env)) : 3;

    cout << "Configuration:" << endl;
    cout << "  TARGET_ADDR: " << target_addr << endl;
    cout << "  TARGET_PORT: " << target_port << endl;
    cout << "  WEBSOCKET_SERVER: " << websocket_server << endl;
    cout << "  RESTART_DELAY: " << restart_delay << " seconds" << endl;

    return SUCCESS;
  }

    int initWebSocket() {
        ws_client.set_access_channels(websocketpp::log::alevel::all);
        ws_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        ws_client.init_asio();

        ws_client.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            // Handle incoming messages if needed
        });

        ws_client.set_open_handler([this](websocketpp::connection_hdl hdl) {
            ws_hdl = hdl;
            connected = true;
            cout << "WebSocket connected" << endl;
        });

        ws_client.set_close_handler([this](websocketpp::connection_hdl hdl) {
            connected = false;
            cout << "WebSocket disconnected" << endl;
        });

        return SUCCESS;
    }

    int connectWebSocket() {
        cout << "Connecting to WebSocket server: " << websocket_server << endl;
        websocketpp::lib::error_code ec;
        client_t::connection_ptr con = ws_client.get_connection(websocket_server, ec);

        if (ec) {
            cout << "Could not create connection: " << ec.message() << endl;
            return ERROR_WEBSOCKET_CONNECT;
        }

        ws_client.connect(con);
        ws_thread = thread([this]() {
            ws_client.run();
        });

        // Wait for connection
        int timeout = 50; // 5 seconds
        while (!connected && timeout-- > 0) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        if (!connected) {
            cout << "WebSocket connection timeout" << endl;
            return ERROR_WEBSOCKET_CONNECT;
        }

        return SUCCESS;
    }

    int sendToWebSocket(const json& message) {
        if (!connected) {
            cout << "WebSocket not connected" << endl;
            return ERROR_WEBSOCKET_SEND;
        }

        websocketpp::lib::error_code ec;
        ws_client.send(ws_hdl, message.dump(), websocketpp::frame::opcode::text, ec);

        if (ec) {
            cout << "WebSocket send error: " << ec.message() << endl;
            return ERROR_WEBSOCKET_SEND;
        }

        return SUCCESS;
    }

    vector<string> formatPacketData(const vector<uint8_t>& data) {
        vector<string> formatted;
        for (uint8_t byte : data) {
            stringstream ss;
            ss << hex << setfill('0') << setw(2) << static_cast<int>(byte);
            formatted.push_back(ss.str());
        }

        // print formatted
        cout << "Packet Data: ";
        for (const auto& byte_str : formatted) {
            cout << byte_str << " ";
        }
        cout << endl;
        
        return formatted;
    }

    int createTCPSocket() {
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            cout << "Socket creation failed" << endl;
            return -1;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(target_port);

        if (inet_pton(AF_INET, target_addr.c_str(), &server_addr.sin_addr) <= 0) {
            cout << "Invalid address: " << target_addr << endl;
            close(sock);
            return -1;
        }

        cout << "Connecting to " << target_addr << ":" << target_port << "...";
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cout << "Connection failed" << endl;
            close(sock);
            return -1;
        }
        cout << "ok." << endl;

        return sock;
    }

    int monitorClient() {
        int ret = connectWebSocket();
        if (ret != SUCCESS) {
            cleanupWebSocket();
            return ret;
        }

        ret = sendToWebSocket({
            {"type", "status"},
            {"message", "Connected to " + target_addr + ":" + to_string(target_port)}
        });
        if (ret != SUCCESS) {
            cleanupWebSocket();
            return ret;
        }

        int sock = createTCPSocket();
        if (sock < 0) {
            cleanupWebSocket();
            return ERROR_SOCKET_CREATE;
        }

        ret = SUCCESS; // Initialize return value for main loop
        while (true) {
            this_thread::sleep_for(chrono::milliseconds(1));

            // Read header (4 bytes)
            uint8_t header[4];
            ssize_t received = recv(sock, header, 4, 0);
            if (received < 4) {
                cout << "recv: head error....ret=" << received << endl;
                close(sock);
                ret = ERROR_RECV_HEADER;
                break;
            }

            uint8_t cmd = header[0];
            uint8_t seq = header[1];
            uint16_t pkt_len = ntohs(*reinterpret_cast<uint16_t*>(&header[2]));

            // Read packet data
            vector<uint8_t> packet_data(header, header + 4);
            int data_len = pkt_len;

            while (data_len > 0) {
                int read_len = min(BLOCK_LEN, data_len);
                vector<uint8_t> chunk(read_len);
                ssize_t chunk_received = recv(sock, chunk.data(), read_len, 0);

                if (chunk_received == 0) {
                    cout << "recv: data error....len=" << read_len << endl;
                    close(sock);
                    ret = ERROR_RECV_DATA;
                    break;
                }

                packet_data.insert(packet_data.end(), chunk.begin(), chunk.begin() + chunk_received);
                data_len -= chunk_received;
            }

            // If data receive error occurred, break from main loop
            if (ret != SUCCESS) {
                break;
            }

            // Format packet data
            auto formatted_data = formatPacketData(packet_data);

            // Determine packet type and source
            string packet_type = "unknown";
            string source = "unknown";

            if (cmd == 0x00 || cmd == 0x05) { // temp fix for ex mode
                packet_type = "plaintext";
                source = "fcc"; // RECV
            }
            if (cmd == 0x01 || cmd == 0x04) { // temp fix for ex mode
                packet_type = "plaintext";
                source = "gcs"; // SEND
            }
            if (cmd == 0x02 || cmd == 0x06) {
                packet_type = "ciphertext";
                source = "fcc"; // encrypt RECV
            }
            if (cmd == 0x03 || cmd == 0x07) {
                packet_type = "ciphertext";
                source = "gcs"; // encrypt SEND
            }
            if (cmd == 16) {
                packet_type = "state";
                source = "dsm";
                // Convert remaining data to string for state packets
                stringstream ss;
                for (size_t i = 4; i < packet_data.size(); i++) {
                    ss << static_cast<char>(packet_data[i]);
                }
                formatted_data = {ss.str()};
            }

            cout << "Received packet: cmd=0x" << hex << static_cast<int>(cmd)
                 << " seq=" << dec << static_cast<int>(seq)
                 << " len=" << pkt_len
                 << " type=" << packet_type
                 << " source=" << source
                 << endl;

            // Send packet info to WebSocket
            json packet_msg = {
                {"type", "packet"},
                {"cmd", cmd},
                {"seq", seq},
                {"length", packet_data.size()},
                {"data", formatted_data},
                {"packet_type", packet_type},
                {"source", source}
            };

            ret = sendToWebSocket(packet_msg);
            if (ret != SUCCESS) {
                cout << "Failed to send packet to WebSocket" << endl;
                // Continue processing other packets ?
            }

            // MAVLink parsing for plaintext packets
            if ((cmd == 0x00 || cmd == 0x01 || cmd == 0x04 || cmd == 0x05) && packet_data.size() > 4) {
                vector<string> mavlink_messages;

                // Parse MAVLink messages from packet data (skip first 4 bytes header in st mode and skip first 28 bytes header in ex mode)
                #define IP_AND_UDP_HEAD_LEN 28
                int header_offset = (cmd <= 0x01) ? 4 : 4 + IP_AND_UDP_HEAD_LEN;
                for (size_t i = header_offset; i < packet_data.size(); i++) {
                    mavlink_message_t msg;
                    mavlink_status_t status;

                    if (mavlink_parse_char(MAVLINK_COMM_0, packet_data[i], &msg, &status)) {
                        // Successfully parsed a MAVLink message, convert to JSON
                        // string msg_name = getMavlinkMessageName(msg.msgid);
                        const mavlink_message_info_t* msg_info = mavlink_get_message_info(&msg);
                        const json mavlink_msg_json = mavlink_get_message_json(&msg);

                        mavlink_messages.push_back(mavlink_msg_json.dump());
                    }
                }

                if (!mavlink_messages.empty()) {
                    json mavlink_msg = {
                        {"type", "packet"},
                        {"cmd", cmd},
                        {"seq", seq},
                        {"data", mavlink_messages},
                        {"packet_type", "mavlink"},
                        {"source", source}
                    };
                    ret = sendToWebSocket(mavlink_msg);
                    if (ret != SUCCESS) {
                        cout << "Failed to send MAVLink message to WebSocket" << endl;
                    }
                }
            }
        }

        // Always clean up resources regardless of how we exit the loop
        close(sock);
        cleanupWebSocket();
        return ret;
    }

  int run() {
    int ret = monitorClient();
    return ret;
  }
};

int main() {

  // Restart loop
  while (true) {
    DSMClient client;
    try {
      int ret = client.initialize();
      if (ret != SUCCESS) {
        cout << "Client initialization failed with error code: " << ret << endl;
        return ret;
      }

      ret = client.run();
      if (ret != SUCCESS) {
        cout << "Client run failed with error code: " << ret << endl;
      }

      client.deinitialize();
      cout << "Restarting client in 3 seconds..." << endl;
      this_thread::sleep_for(chrono::seconds(3));

    } catch (const exception& e) {
      cout << "Exception: " << e.what() << endl;

      client.deinitialize();
      cout << "Restarting client in 3 seconds..." << endl;
      this_thread::sleep_for(chrono::seconds(3));
    }
  }

  return SUCCESS;
}

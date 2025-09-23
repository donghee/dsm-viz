#!/usr/bin/env python3
import os
import socket
import struct
import asyncio
import websockets
import json
import time
from dotenv import load_dotenv
from pymavlink import mavutil

load_dotenv()

TARGET_ADDR = os.getenv('TARGET_ADDR', '127.0.0.1')
TARGET_PORT = int(os.getenv('TARGET_PORT', 14445))
WEBSOCKET_SERVER = os.getenv('WEBSOCKET_SERVER', 'ws://127.0.0.1:8000/ws')
RESTART_DELAY = int(os.getenv('RESTART_DELAY', 3))  # 재시작 대기 시간 (초)

BUFF_LEN = 32 * 1024
BLOCK_LEN = 4 * 1024

mav = mavutil.mavlink.MAVLink(None)
mav.srcSystem = 1
mav.srcComponent = 1

def format_packet_data(data):
    """패킷 데이터를 출력용으로 포맷팅"""
    output = []
    for i in range(0, len(data), 16):
        hex_part = ' '.join(f'{b:02x}' for b in data[i:i+16])
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
        output.append(f'{i:04x}: {hex_part:<47} {ascii_part}')
    return '\n'.join(output)

async def send_to_websocket(websocket, message):
    """웹소켓으로 메시지 전송"""
    try:
        await websocket.send(json.dumps(message))
    except TypeError as e:
            print(f"WebSocket send error: {e}")
    except Exception as e:
        print(f"WebSocket send error: {e}")
        raise  # 예외를 다시 발생시켜 상위에서 처리하도록 함

async def monitor_client():
    """모니터링 클라이언트 메인 함수"""
    websocket = None

    try:
        # 웹소켓 서버 연결
        print(f"Connecting to WebSocket server: {WEBSOCKET_SERVER}")
        websocket = await websockets.connect(WEBSOCKET_SERVER)
        print("WebSocket connected")

        # TCP 소켓 생성 및 연결
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        print(f"Connecting to {TARGET_ADDR}:{TARGET_PORT}...", end="")
        sock.connect((TARGET_ADDR, TARGET_PORT))
        print("ok.")

        await send_to_websocket(websocket, {
            "type": "status",
            "message": f"Connected to {TARGET_ADDR}:{TARGET_PORT}"
        })

        while True:
            # 헤더 읽기 (4 bytes)
            header = sock.recv(4)
            if len(header) < 4:
                print(f"recv: head error....ret={len(header)}")
                break

            cmd, seq, pkt_len = struct.unpack('BBH', header)
            pkt_len = socket.ntohs(pkt_len)
            print(f"read head cmd={cmd} seq={seq} pkt_len={pkt_len}")

            # 패킷 데이터 읽기
            data_len = pkt_len
            packet_data = header

            while data_len > 0:
                read_len = min(BLOCK_LEN, data_len)
                chunk = sock.recv(read_len)
                if len(chunk) == 0:
                    print(f"recv: data error....len={read_len}")
                    return
                packet_data += chunk
                data_len -= len(chunk)

            # 패킷 출력 및 웹소켓 전송
            formatted_data = format_packet_data(packet_data)
            print(f"Packet received (len={len(packet_data)}):")
            #  print(formatted_data)

            packet_type = "unknown"
            source = "unknown"
            if cmd == 0x01 or cmd == 0x05:
                packet_type = "plaintext"
                source = "gcs"
            if cmd == 0x03 or cmd == 0x07:
                packet_type = "ciphertext"
                source = "gcs"
            if cmd == 0x02 or cmd == 0x06:
                packet_type = "ciphertext"
                source = "fcc"
            if cmd == 0x00 or cmd == 0x04:
                packet_type = "plaintext"
                source = "fcc"
            if cmd == 16:
                packet_type = "state"
                source = "dsm"
                formatted_data = str(packet_data[4:])

            await send_to_websocket(websocket, {
                "type": "packet",
                "cmd": cmd,
                "seq": seq,
                "length": len(packet_data),
                "data": formatted_data,
                "packet_type": packet_type,
                "source": source
            })

            # convert packet_data to mavlink json messages
            if (cmd == 0x00 or cmd == 0x04 or cmd == 0x01 or cmd == 0x05) and len(packet_data) > 4:
                mavlink_data = packet_data[4:]  # 헤더(4 bytes) 제거
                messages = []

                for byte_val in mavlink_data:
                    msg = mav.parse_char(bytes([byte_val]))
                    if msg:
                        messages.append(msg.to_dict())

                if messages:
                    await send_to_websocket(websocket, {
                        "type": "packet",
                        "cmd": cmd,
                        "seq": seq,
                        "data": messages,
                        "packet_type": "mavlink",
                        "source": source
                    })
                    print(f"Parsed {len(messages)} MAVLink messages")


    except KeyboardInterrupt:
        print("\nStopping client...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if websocket:
            await websocket.close()

async def main():
    """메인 함수 - 클라이언트 재시작 로직 포함"""
    while True:
        try:
            await monitor_client()
        except KeyboardInterrupt:
            print("\nStopping client...")
            break
        except Exception as e:
            print(f"Client error: {e}")
            print(f"Restarting client in {RESTART_DELAY} seconds...")
            await asyncio.sleep(RESTART_DELAY)

if __name__ == "__main__":
    asyncio.run(main())

#!/usr/bin/env python3
import os
import asyncio
import websockets
import json
from pathlib import Path
from aiohttp import web, WSMsgType
from aiohttp.web_ws import WebSocketResponse
from dotenv import load_dotenv

load_dotenv()

HOST = os.getenv('HOST', '0.0.0.0')
WEB_PORT = int(os.getenv('WEB_PORT', 8000))
WEBSOCKET_PORT = int(os.getenv('WEBSOCKET_PORT', 8000))

# 연결된 WebSocket 클라이언트들 저장
websocket_clients = set()

async def websocket_handler(request):
    global websocket_clients
    """WebSocket 핸들러"""
    ws = WebSocketResponse()
    await ws.prepare(request)

    websocket_clients.add(ws)
    print(f"Client connected. Total clients: {len(websocket_clients)}")

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                data = json.loads(msg.data)
                print(f"Received: {data}")

                # 모든 연결된 클라이언트에게 브로드캐스트
                disconnected_clients = set()
                for client in websocket_clients:
                    try:
                        await client.send_str(msg.data)
                    except Exception:
                        disconnected_clients.add(client)

                # 끊어진 클라이언트 제거
                websocket_clients -= disconnected_clients

            elif msg.type == WSMsgType.ERROR:
                print(f'WebSocket error: {ws.exception()}')
                break
    except Exception as e:
        print(f"WebSocket handler error: {e}")
    finally:
        websocket_clients.discard(ws)
        print(f"Client disconnected. Total clients: {len(websocket_clients)}")

    return ws

async def index_handler(request):
    html_content = """
<!DOCTYPE html>
<html>
<head>
    <title>패킷 모니터링 시각화</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://d3js.org/d3.v7.min.js"></script>
    <script>
        tailwind.config = {
            theme: {
                extend: {
                    colors: {
                        'light-bg': '#f8fafc',
                        'light-text': '#1e293b',
                        'light-card': '#ffffff',
                        'light-border': '#e2e8f0',
                        'light-accent': '#3b82f6',
                        'light-success': '#059669',
                        'light-warning': '#d97706',
                        'light-error': '#dc2626'
                    },
                    fontFamily: {
                        'mono': ['Courier New', 'monospace']
                    }
                }
            }
        }
    </script>
</head>
<body class="font-mono bg-light-bg text-light-text p-5">
<!-- logo -->
<div class="flex items-center justify-between mb-5">
<img src="https://www.etri.re.kr/images/kor/sub5/ci_img01.png" alt="한국전자통신연구원" class="h-12">
<h1 class="text-light-text text-2xl font-bold flex-1 text-center">DSM 보안 채널 모니터링</h1>
<div class="h-12 w-12"></div> <!-- 균형을 위한 빈 공간 -->
</div>

    <!-- Tab Navigation -->
    <div class="mb-4">
        <nav class="flex space-x-1 bg-light-card border border-light-border p-1 rounded-lg shadow-sm">
            <button id="state-tab" class="flex-1 py-2 px-4 text-sm font-medium text-center rounded-md transition-colors duration-200 bg-light-accent text-white" onclick="switchTab('state')">
                State Information
            </button>
            <button id="fcc-tab" class="flex-1 py-2 px-4 text-sm font-medium text-center rounded-md transition-colors duration-200 text-gray-600 hover:text-light-accent hover:bg-gray-50" onclick="switchTab('fcc')">
                Data Monitoring: FCC -> GCS
            </button>
            <button id="gcs-tab" class="flex-1 py-2 px-4 text-sm font-medium text-center rounded-md transition-colors duration-200 text-gray-600 hover:text-light-accent hover:bg-gray-50" onclick="switchTab('gcs')">
                Data Monitoring: GCS -> FCC 
            </button>
            <button id="fcc-graph-tab" class="flex-1 py-2 px-4 text-sm font-medium text-center rounded-md transition-colors duration-200 text-gray-600 hover:text-light-accent hover:bg-gray-50" onclick="switchTab('fcc-graph')">
                Visualization: FCC -> GCS
            </button>
             <button id="gcs-graph-tab" class="flex-1 py-2 px-4 text-sm font-medium text-center rounded-md transition-colors duration-200 text-gray-600 hover:text-light-accent hover:bg-gray-50" onclick="switchTab('gcs-graph')">
                Visualization: GCS -> FCC 
            </button>
        </nav>
    </div>

    <!-- Tab Content -->
    <div id="state-content" class="tab-content">
        <div class="grid grid-cols-2 gap-5 h-[80vh]">
            <!-- Connection Status -->
            <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
                <h3 class="text-light-text font-bold mb-3 text-center">Connection Status</h3>
                <div class="space-y-2">
                    <div class="flex justify-between">
                        <span class="text-gray-600">Connection:</span>
                        <span id="dsm-status" class="text-light-error">Disconnected</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">TLS Version:</span>
                        <span id="dsm-tls-version" class="text-light-accent"></span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">KEM:</span>
                        <span id="dsm-kem" class="text-light-accent">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">SIG:</span>
                        <span id="dsm-sig" class="text-light-accent">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">Ciphersuite:</span>
                        <span id="dsm-ciphersuite" class="text-light-accent">0</span>
                    </div>
                </div>
            </div>

            <!-- System Statistics -->
            <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
                <h3 class="text-light-text font-bold mb-3 text-center">System Statistics</h3>
                <div class="space-y-2">
                    <div class="flex justify-between">
                        <span class="text-gray-600">TX Packets:</span>
                        <span id="tx-packets" class="text-light-accent">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">RX Packets:</span>
                        <span id="rx-packets" class="text-light-accent">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">TX Bytes:</span>
                        <span id="tx-bytes" class="text-light-accent">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">RX Bytes:</span>
                        <span id="rx-bytes" class="text-light-accent">0</span>
                    </div>
                </div>
            </div>

            <!-- Recent Activity Log -->
            <div class="col-span-2 bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
                <h3 class="text-light-text font-bold mb-3 text-center">Recent Activity</h3>
                <textarea id="active-log" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent" readonly></textarea>
            </div>
        </div>
    </div>

    <div id="fcc-content" class="tab-content hidden flex gap-5 h-[80vh]">
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border p-2 rounded-t-lg">Ciphertext</div>
            <textarea id="fcc-ciphertext" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border p-2 rounded-t-lg">Plaintext</div>
            <textarea id="fcc-plaintext" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border
            p-2 rounded-t-lg">MAVLink</div>
            <textarea id="fcc-mavlink" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
    </div>

    <div id="fcc-graph-content" class="tab-content hidden flex flex-col gap-5 h-[80vh] overflow-y-auto">
        <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
            <h3 class="text-light-text font-bold mb-3 text-center">Packet Entropy Analysis - FCC → GCS</h3>
            <div id="fcc-entropy-chart" class="w-full h-96"></div>
        </div>
        <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
            <h3 class="text-light-text font-bold mb-3 text-center">Chi-square Analysis - FCC → GCS</h3>
            <div id="fcc-chisquare-chart" class="w-full h-96"></div>
        </div>
    </div>

    <div id="gcs-graph-content" class="tab-content hidden flex flex-col gap-5 h-[80vh] overflow-y-auto">
        <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
            <h3 class="text-light-text font-bold mb-3 text-center">Packet Entropy Analysis - GCS → FCC</h3>
            <div id="gcs-entropy-chart" class="w-full h-96"></div>
        </div>
        <div class="bg-light-card border border-light-border p-4 rounded-lg shadow-sm">
            <h3 class="text-light-text font-bold mb-3 text-center">Chi-square Analysis - GCS → FCC</h3>
            <div id="gcs-chisquare-chart" class="w-full h-96"></div>
        </div>
    </div>


    <div id="gcs-content" class="tab-content hidden flex gap-5 h-[80vh]">
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border p-2 rounded-t-lg">MAVLink</div>
            <textarea id="gcs-mavlink" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border p-2 rounded-t-lg">Plaintext</div>
            <textarea id="gcs-plaintext" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
        <div class="flex-1 flex flex-col">
            <div class="text-light-text font-bold mb-1 text-center bg-light-card border border-light-border p-2 rounded-t-lg">Ciphertext</div>
            <textarea id="gcs-ciphertext" class="w-full h-full bg-white text-light-text font-mono text-xs border border-light-border border-t-0 p-2 overflow-y-auto whitespace-pre-wrap resize-none focus:outline-none focus:border-light-accent rounded-b-lg" readonly></textarea>
        </div>
    </div>

    <script>
        // Tab switching functionality
        function switchTab(tabName) {
            // Hide all tab contents
            const tabContents = document.querySelectorAll('.tab-content');
            tabContents.forEach(content => {
                content.classList.add('hidden');
            });

            // Show selected tab content
            const selectedContent = document.getElementById(tabName + '-content');
            selectedContent.classList.remove('hidden');

            // Update tab button styles
            const allTabs = document.querySelectorAll('[id$="-tab"]');
            allTabs.forEach(tab => {
                tab.classList.remove('bg-light-accent', 'text-white');
                tab.classList.add('text-gray-600', 'hover:text-light-accent', 'hover:bg-gray-50');
            });

            // Activate selected tab
            const selectedTab = document.getElementById(tabName + '-tab');
            selectedTab.classList.remove('text-gray-600', 'hover:text-light-accent', 'hover:bg-gray-50');
            selectedTab.classList.add('bg-light-accent', 'text-white');
        }

        // Initialize tab elements
        const fccCiphertext = document.getElementById('fcc-ciphertext');
        const fccPlaintext = document.getElementById('fcc-plaintext');
        const fccMavlink = document.getElementById('fcc-mavlink');

        const gcsCiphertext = document.getElementById('gcs-ciphertext');
        const gcsPlaintext = document.getElementById('gcs-plaintext');
        const gcsMavlink = document.getElementById('gcs-mavlink');

        const ws = new WebSocket(`ws://${window.location.host}/ws`);

        // 최대 라인 수 제한 (성능 최적화)
        const MAX_LINES = 100;

        function addMessage(textarea, message, className = '') {
            const timestamp = new Date().toLocaleTimeString();
            const line = `[${timestamp}] ${message}\\n`;

            let content = '';
            if (className) {
                const prefix = className === 'status' ? '[STATUS] ' :
                              className === 'packet-header' ? '[PACKET] ' : '';
                content = prefix + line;
            } else {
                content = line;
            }

            // 현재 내용에 새 라인 추가
            textarea.value += content;

            // 라인 수 체크 및 오래된 라인 제거 (성능 최적화)
            const lines = textarea.value.split('\\n');
            if (lines.length > MAX_LINES) {
                const linesToRemove = lines.length - MAX_LINES;
                textarea.value = lines.slice(linesToRemove).join('\\n');
            }

            // 자동 스크롤 최적화 (사용자가 스크롤을 올리지 않은 경우에만)
            //const isAtBottom = textarea.scrollTop >= textarea.scrollHeight - textarea.clientHeight - 10;
            //if (isAtBottom) {
                //textarea.scrollTop = textarea.scrollHeight;
            //}
        }

        ws.onopen = function(event) {
            // 모든 textarea에 연결 상태 메시지 추가
            addMessage(fccCiphertext, 'WebSocket 연결됨', 'status');
            addMessage(fccPlaintext, 'WebSocket 연결됨', 'status');
            addMessage(fccMavlink, 'WebSocket 연결됨', 'status');
            addMessage(gcsCiphertext, 'WebSocket 연결됨', 'status');
            addMessage(gcsPlaintext, 'WebSocket 연결됨', 'status');
            addMessage(gcsMavlink, 'WebSocket 연결됨', 'status');
        };

        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);

            if (data.type === 'status') {
                // 모든 textarea에 상태 메시지 추가
                addMessage(fccCiphertext, data.message, 'status');
                addMessage(fccPlaintext, data.message, 'status');
                addMessage(fccMavlink, data.message, 'status');
                addMessage(gcsCiphertext, data.message, 'status');
                addMessage(gcsPlaintext, data.message, 'status');
                addMessage(gcsMavlink, data.message, 'status');
            } else if (data.type === 'packet') {
                // 패킷의 소스(FCC/GCS)와 타입에 따라 적절한 textarea에 출력
                const source = data.source || 'fcc'; // 기본값은 fcc

                if (data.packet_type === 'state') {
                    // data.data type print
                    // 상태 정보는 active-log에 출력
                    const activeLog = document.getElementById('active-log');
                    addMessage(activeLog, `[${source.toUpperCase()}] ${data.data}`, 'status');
                    let cleanedState = {};
                    if (typeof data.data === 'string') {
                        // prepare string for JSON parsing
                        cleanedState = data.data.trim().replace(/^b['"]/, '').replace(/['"]$/, '');

                    } else  {
                        // data.data[0] is already a object
                        cleanedState = data.data[0];
                    }
                    JSON.parse(cleanedState, (key, value) => {
                        // console.log(`Key: ${key}, Value: ${value}`);
                        if (key === 'state') {
                            document.getElementById('dsm-status').innerText = value;
                        }
                        if (key === 'tls_ver') {
                            document.getElementById('dsm-tls-version').innerText = value;
                        }
                        if (key === 'kem') {
                            document.getElementById('dsm-kem').innerText = value;
                        }
                        if (key === 'sig') {
                            document.getElementById('dsm-sig').innerText = value;
                        }
                        if (key === 'ciphersuite') {
                            document.getElementById('dsm-ciphersuite').innerText = value;
                        }
                        if (key === 'tx_packets') {
                            document.getElementById('tx-packets').innerText = value;
                        }
                        if (key === 'rx_packets') {
                            document.getElementById('rx-packets').innerText = value;
                        }
                        if (key === 'tx_bytes') {
                            document.getElementById('tx-bytes').innerText = value;
                        }
                        if (key === 'rx_bytes') {
                            document.getElementById('rx-bytes').innerText = value;
                        }
                    });
                    return;
                }
                if (data.packet_type === 'ciphertext') {
                    const targetTextarea = source === 'fcc' ? fccCiphertext : gcsCiphertext;
                    // addMessage(targetTextarea, `CMD=${data.cmd} SEQ=${data.seq} LEN=${data.length}`, 'packet-header');
                    addMessage(targetTextarea, data.data, 'packet-data');

                    // Calculate and store entropy for packets
                    if (source === 'fcc') {
                        //console.log(data.data);
                        const entropy = calculateEntropy(data.data);
                        // console.log(`Entropy: ${entropy}`);
                        entropyData.ciphertext.push(entropy);
                        entropyData.timestamps.push(new Date());

                        // Calculate and store chi-square for packets
                        const chiSquare = calculateChiSquare(data.data);
                        // console.log(`Chi-Square: ${chiSquare}`);
                        chiSquareData.ciphertext.push(chiSquare);

                        // Limit data points for performance
                        if (entropyData.ciphertext.length > maxDataPoints) {
                            entropyData.ciphertext.shift();
                            entropyData.timestamps.shift();
                            chiSquareData.ciphertext.shift();
                        }
                    } else if (source === 'gcs') {
                        if (data.data === null || data.data.length === 0) return;
                        const entropy = calculateEntropy(data.data);
                        gcsEntropyData.ciphertext.push(entropy);
                        gcsEntropyData.timestamps.push(new Date());

                        // Calculate and store chi-square for packets
                        const chiSquare = calculateChiSquare(data.data);
                        gcsChiSquareData.ciphertext.push(chiSquare);

                        // Limit data points for performance
                        if (gcsEntropyData.ciphertext.length > maxDataPoints) {
                            gcsEntropyData.ciphertext.shift();
                            gcsEntropyData.timestamps.shift();
                            gcsChiSquareData.ciphertext.shift();
                        }
                    }
                    updateEntropyChart(source);
                    updateChiSquareChart(source);
                } else if (data.packet_type === 'plaintext') {
                    const targetTextarea = source === 'fcc' ? fccPlaintext : gcsPlaintext;
                    //addMessage(targetTextarea, `CMD=${data.cmd} SEQ=${data.seq} LEN=${data.length}`, 'packet-header');
                    addMessage(targetTextarea, data.data, 'packet-data');

                    // Calculate and store entropy for packets
                    if (source === 'fcc') {
                        const entropy = calculateEntropy(data.data);
                        entropyData.plaintext.push(entropy);

                        // Calculate and store chi-square for packets
                        const chiSquare = calculateChiSquare(data.data);
                        chiSquareData.plaintext.push(chiSquare);

                        // Limit data points for performance
                        if (entropyData.plaintext.length > maxDataPoints) {
                            entropyData.plaintext.shift();
                            chiSquareData.plaintext.shift();
                            if (entropyData.plaintext.length < entropyData.ciphertext.length) {
                                entropyData.ciphertext.shift();
                                chiSquareData.ciphertext.shift();
                            }
                        }
                    } else if (source === 'gcs') {
                        const entropy = calculateEntropy(data.data);
                        gcsEntropyData.plaintext.push(entropy);

                        // Calculate and store chi-square for packets
                        const chiSquare = calculateChiSquare(data.data);
                        gcsChiSquareData.plaintext.push(chiSquare);

                        // Limit data points for performance
                        if (gcsEntropyData.plaintext.length > maxDataPoints) {
                            gcsEntropyData.plaintext.shift();
                            gcsChiSquareData.plaintext.shift();
                            if (gcsEntropyData.plaintext.length < gcsEntropyData.ciphertext.length) {
                                gcsEntropyData.ciphertext.shift();
                                gcsChiSquareData.ciphertext.shift();
                            }
                        }

                    }
                    updateEntropyChart(source);
                    updateChiSquareChart(source);
                } else if (data.packet_type === 'mavlink') {
                    const targetTextarea = source === 'fcc' ? fccMavlink : gcsMavlink;
                    //addMessage(targetTextarea, `CMD=${data.cmd} SEQ=${data.seq} LEN=${data.length}`, 'packet-header');
                    addMessage(targetTextarea, JSON.stringify(data.data, null, 2), 'packet-data');
                } else {
                    // 기본적으로 FCC ciphertext에 출력
                    addMessage(fccCiphertext, `CMD=${data.cmd} SEQ=${data.seq} LEN=${data.length}`, 'packet-header');
                    addMessage(fccCiphertext, data.data, 'packet-data');
                }
            }
        };

        ws.onclose = function(event) {
            addMessage(fccCiphertext, 'WebSocket 연결 종료', 'status');
            addMessage(fccPlaintext, 'WebSocket 연결 종료', 'status');
            addMessage(fccMavlink, 'WebSocket 연결 종료', 'status');
            addMessage(gcsCiphertext, 'WebSocket 연결 종료', 'status');
            addMessage(gcsPlaintext, 'WebSocket 연결 종료', 'status');
            addMessage(gcsMavlink, 'WebSocket 연결 종료', 'status');
        };

        ws.onerror = function(error) {
            addMessage(fccCiphertext, 'WebSocket 오류: ' + error, 'status');
            addMessage(fccPlaintext, 'WebSocket 오류: ' + error, 'status');
            addMessage(fccMavlink, 'WebSocket 오류: ' + error, 'status');
            addMessage(gcsCiphertext, 'WebSocket 오류: ' + error, 'status');
            addMessage(gcsPlaintext, 'WebSocket 오류: ' + error, 'status');
            addMessage(gcsMavlink, 'WebSocket 오류: ' + error, 'status');
        };

        // Entropy calculation and visualization
        let entropyData = {
            plaintext: [],
            ciphertext: [],
            timestamps: []
        };

        let gcsEntropyData = {
            plaintext: [],
            ciphertext: [],
            timestamps: []
        };

        let chiSquareData = {
            plaintext: [],
            ciphertext: [],
            timestamps: []
        };

        let gcsChiSquareData = {
            plaintext: [],
            ciphertext: [],
            timestamps: []
        };

        let maxDataPoints = 50;

        function calculateEntropy(data) {
            if (!data || data.length === 0) return 0;

            // Convert string to bytes if needed
            const bytes = typeof data === 'string' ?
                new TextEncoder().encode(data) : data;

            // Count frequency of each byte value
            const frequencies = new Array(256).fill(0);
            for (let i = 0; i < bytes.length; i++) {
                frequencies[bytes[i]]++;
            }

            // Calculate entropy using Shannon formula
            let entropy = 0.0;
            const len = bytes.length;
            for (let i = 0; i < 256; i++) {
                if (frequencies[i] > 0) {
                    const prob = frequencies[i] / len;
                    entropy -= prob * Math.log2(prob);
                }
            }

            return entropy;
        }

        function calculateChiSquare(data) {
            if (!data || data.length === 0) return 0;

            // Convert string to bytes if needed
            const bytes = typeof data === 'string' ?
                new TextEncoder().encode(data) : data;

            if (bytes.length === 0) return 0;

            // Count frequency of each byte value (0-255)
            const observed = new Array(256).fill(0);
            for (let i = 0; i < bytes.length; i++) {
                observed[bytes[i]]++;
            }

            // Expected frequency for uniform distribution
            const expected = bytes.length / 256;

            // Calculate chi-square statistic
            let chiSquare = 0.0;
            for (let i = 0; i < 256; i++) {
                if (expected > 0) {
                    const diff = observed[i] - expected;
                    chiSquare += (diff * diff) / expected;
                }
            }

            return chiSquare;
        }

        // Generic chart update function to reduce code duplication
        function updateChart(config) {
            const margin = {top: 20, right: 80, bottom: 30, left: 50};
            const width = document.getElementById(config.chartId).clientWidth - margin.left - margin.right;
            const height = 350 - margin.top - margin.bottom;

            // Clear previous chart
            d3.select(`#${config.chartId}`).selectAll("*").remove();

            const svg = d3.select(`#${config.chartId}`)
                .append("svg")
                .attr("width", width + margin.left + margin.right)
                .attr("height", height + margin.top + margin.bottom)
                .append("g")
                .attr("transform", `translate(${margin.left},${margin.top})`);

            if (config.data.plaintext.length === 0) {
                svg.append("text")
                    .attr("x", width / 2)
                    .attr("y", height / 2)
                    .attr("text-anchor", "middle")
                    .style("font-size", "16px")
                    .style("fill", "#6b7280")
                    .text("No data available");
                return;
            }

            // Scales
            const xScale = d3.scaleLinear()
                .domain([0, Math.max(config.data.plaintext.length - 1, 1)])
                .range([0, width]);

            const maxValue = Math.max(
                d3.max(config.data.plaintext) || 0,
                d3.max(config.data.ciphertext) || 0,
                config.defaultMaxValue
            );

            const yScale = d3.scaleLinear()
                .domain([0, maxValue])
                .range([height, 0]);

            // Line generators
            const plainLine = d3.line()
                .x((d, i) => xScale(i))
                .y(d => yScale(d))
                .curve(d3.curveMonotoneX);

            const cipherLine = d3.line()
                .x((d, i) => xScale(i))
                .y(d => yScale(d))
                .curve(d3.curveMonotoneX);

            // Add axes
            svg.append("g")
                .attr("transform", `translate(0,${height})`)
                .call(d3.axisBottom(xScale));

            svg.append("g")
                .call(d3.axisLeft(yScale));

            // Add axis labels
            svg.append("text")
                .attr("transform", "rotate(-90)")
                .attr("y", 0 - margin.left)
                .attr("x", 0 - (height / 2))
                .attr("dy", "1em")
                .style("text-anchor", "middle")
                .style("font-size", "12px")
                .text(config.yAxisLabel);

            svg.append("text")
                .attr("transform", `translate(${width / 2}, ${height + margin.bottom})`)
                .style("text-anchor", "middle")
                .style("font-size", "12px")
                .text("Packet Number");

            // Add lines
            if (config.data.plaintext.length > 0) {
                svg.append("path")
                    .datum(config.data.plaintext)
                    .attr("fill", "none")
                    .attr("stroke", "#10b981")
                    .attr("stroke-width", 2)
                    .attr("d", plainLine);
            }

            if (config.data.ciphertext.length > 0) {
                svg.append("path")
                    .datum(config.data.ciphertext)
                    .attr("fill", "none")
                    .attr("stroke", "#ef4444")
                    .attr("stroke-width", 2)
                    .attr("d", cipherLine);
            }

            // Add legend
            const legend = svg.append("g")
                .attr("transform", `translate(${width - 100}, 20)`);

            legend.append("line")
                .attr("x1", 0)
                .attr("x2", 20)
                .attr("y1", 0)
                .attr("y2", 0)
                .attr("stroke", "#10b981")
                .attr("stroke-width", 2);

            legend.append("text")
                .attr("x", 25)
                .attr("y", 0)
                .attr("dy", "0.35em")
                .style("font-size", "12px")
                .text("Plaintext");

            legend.append("line")
                .attr("x1", 0)
                .attr("x2", 20)
                .attr("y1", 20)
                .attr("y2", 20)
                .attr("stroke", "#ef4444")
                .attr("stroke-width", 2);

            legend.append("text")
                .attr("x", 25)
                .attr("y", 20)
                .attr("dy", "0.35em")
                .style("font-size", "12px")
                .text("Ciphertext");
        }

        function updateEntropyChart(source) {
            const data = source === 'fcc' ? entropyData : gcsEntropyData;
            updateChart({
                chartId: `${source}-entropy-chart`,
                data: data,
                yAxisLabel: 'Entropy (bits)',
                defaultMaxValue: 3
            });
        }

        function updateChiSquareChart(source) {
            const data = source === 'fcc' ? chiSquareData : gcsChiSquareData;
            updateChart({
                chartId: `${source}-chisquare-chart`,
                data: data,
                yAxisLabel: 'Chi-square Value',
                defaultMaxValue: 200
            });
        }

        // Initialize charts
        updateEntropyChart('fcc');
        updateEntropyChart('gcs');
        updateChiSquareChart('fcc');
        updateChiSquareChart('gcs');
    </script>
</body>
</html>
    """
    return web.Response(text=html_content, content_type='text/html')

def create_app():
    app = web.Application()
    app.router.add_get('/', index_handler)
    app.router.add_get('/ws', websocket_handler)
    return app

async def main():
    print(f"Starting visualizing server on {HOST}:{WEB_PORT}")

    app = create_app()
    runner = web.AppRunner(app)
    await runner.setup()

    site = web.TCPSite(runner, HOST, WEB_PORT)
    await site.start()

    print(f"Web server running on http://{HOST}:{WEB_PORT}")
    print(f"WebSocket server running on ws://{HOST}:{WEB_PORT}/ws")
    print("Press Ctrl+C to stop")

    try:
        await asyncio.Future()  # run forever
    except KeyboardInterrupt:
        print("\nStopping server...")
    finally:
        await runner.cleanup()

if __name__ == "__main__":
    asyncio.run(main())

#!/bin/sh

set -e

APP_PATH=$(dirname "$(realpath "$0")")/..

mkdir -p $APP_PATH/log

# Start QGroundControl in background
$APP_PATH/external/QGroundControl-x86_64.AppImage >> $APP_PATH/log/qgroundcontrol.log 2>&1 &
echo "QGroundControl started"
sleep 5

# Start simulator
#DUMP_FILE=$APP_PATH/server/simulator/record_dsm_st_client_20250817a.dump
DUMP_FILE=$APP_PATH/server/simulator/record_dsm_st_server_20250817a.dump
#DUMP_FILE=$APP_PATH/server/simulator/record_dsm_ex_client_20250815a.dump
DUMP_FILE=$APP_PATH/server/simulator/record_dsm_ex_server_20250815a.dump
$APP_PATH/server/simulator/sim_mon_srv $DUMP_FILE >> $APP_PATH/log/simulator.log 2>&1 &
SIMULATOR_PID=$!
echo "Simulator started with PID $SIMULATOR_PID"
sleep 1

# Start dsm-viz server in background and client in foreground
python3 $APP_PATH/server/server.py >> $APP_PATH/log/server.log 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"
sleep 1

# Catch interrupt signal
trap "set +e; \
      echo 'Stopping QGroundControl ...;' ; \
      pkill -f QGroundControl; \
      echo 'Stopping simulator pid: $SIMULATOR_PID...;' \
      kill -9 $SIMULATOR_PID; \
      pkill -f sim_mon_srv; \
      echo 'Stopping server pid: $SERVER_PID ...;' \
      kill -9 $SERVER_PID; \
      pkill -f server.py; \
      wait; \
      echo 'QGroundControl and Server and simulator stopped.'; \
      echo 'Logs are available in $APP_PATH/log/'; \
      exit 0" INT

TARGET_ADDR=127.0.0.1 TARGET_PORT=14445 $APP_PATH/client/build/client

echo "Stopping QGroundControl ...;"
pkill -f QGroundControl
echo "Stopping simulator"
pkill -f sim_mon_srv
echo "Stopping server ..."
pkill -f server.py
wait
echo "QGroundControl and Server and simulator stopped."
echo "Logs are available in $APP_PATH/log/"
exit 0

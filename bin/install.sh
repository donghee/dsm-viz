#!/bin/sh

set -e

APP_PATH=$(dirname "$(realpath "$0")")/..

pip install -r $APP_PATH/server/requirements.txt --user --break-system-packages

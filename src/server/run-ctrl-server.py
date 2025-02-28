#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR
me=$(basename ${BASH_SOURCE[0]})

if command -v distrobox-enter; then
    exec distrobox-enter tumbleweed -- "$SCRIPT_DIR/$me"
    exit 0
fi

pid=-1
running=1

for i in python3 inotifywait; do
        if ! command -v $i; then
            echo "WARN: installing $i"
            sudo zypper in $i || exit 2
        fi
done

function ctrl_c() {
    echo "Stopping! ++++++"
    httpd_kill
    running=0
}
trap ctrl_c INT

if [ ! -d .venv ]; then
    python3 -m venv .venv
fi

source .venv/bin/activate
pip install simple-http-server
pip install numpy
pip install requests



httpd_start()
{
    if [ $pid -eq -1 ]; then
        echo "Start httpd"
        python3 ./ctrl-server.py &
        pid=$!
    fi
}

httpd_kill()
{
    if [ $pid -ne -1 ]; then
        echo "Kill httpd with pid $pid"
        kill $pid
        pid=-1
    fi
}

httpd_start;

while [ $running -eq 1 ]; do
    echo inotifywait -r -e modify $SCRIPT_DIR/ctrl-server.py $SCRIPT_DIR/lib
    inotifywait -r -e modify $SCRIPT_DIR/ctrl-server.py $SCRIPT_DIR/lib

    echo "RELOAD server"
    httpd_kill
    httpd_start
    sleep 1;
done

httpd_kill
popd


#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR

me=$0
pid=-1
running=1

for i in python3 inotifywait; do
    if ! command -v $i; then
        echo "ERROR: pleace install $i!"
        exit 2
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

httpd_start()
{
    if [ $pid -eq -1 ]; then
        echo "Start httpd"
        touch $me
        python3 ./http-dev-server.py &
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
    echo inotifywait -r -e modify ../src/data_src/ http-dev-server.py
    inotifywait -r -e modify ../src/data_src/ http-dev-server.py

    echo "RELOAD http-dev-server.py"
    httpd_kill
    httpd_start
    sleep 1;
done

httpd_kill
popd


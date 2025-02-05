#!/bin/bash
need_build=${force:-0}
me="./build.sh"
pio=pio
check_folders="src data_src"
esp_idf_monitor=1

if command -v distrobox-enter; then
    pio="distrobox-enter tumbleweed -- pio "
fi

print_help()
{
      echo "Usage:"
      echo "  --force -f     force build"
      echo "  --monitor      Only run monitor"
      echo "  --[no-]idf-monitor  Use idf_monitor.py"
      echo ""
}

for dir in $check_folders; do
  [[ "$need_build" -eq 1 ]] && break;
  [[ ! -d "$dir" ]] && continue;

	for f in $(find $dir); do
		if [ "$f" -nt "$me" ]; then
			need_build=1;
			break;
		fi
	done
done

while [[ $# -gt 0 ]]; do
  case $1 in
    --force|-f)
      need_build=1;
      shift
      ;;
    --monitor)
      need_build=0;
      shift
      ;;
    --help|-h)
      print_help
      exit 0;
      ;;
    --idf-monitor)
      esp_idf_monitor=1
      shift
      ;;
    --no-idf-monitor)
      esp_idf_monitor=0
      shift
      ;;
    *)
      echo "Invalid argument"
      echo ""
      print_help
      exit 2;
      ;;
  esac
done

function monitor()
{
    if [ "$esp_idf_monitor" -eq 1 ]; then
        echo "IDF MONITOR"
        source ~/.platformio/penv/.espidf-5.3.0/bin/activate
        python3 /home/clemix/.platformio/packages/framework-espidf/tools/idf_monitor.py \
            --port /dev/ttyUSB0 \
            --baud 115200 \
            --toolchain-prefix /home/clemix/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf- \
            .pio/build/esp32dev/firmware.elf
    else
        $pio device monitor
    fi
}

if [ $need_build -eq 1 ]; then
    # universal-ctags -R --exclude='.*.swp' --exclude=.svn \
    #     --exclude=.git --exclude='*~' --c++-kinds=+p --fields=+iaS \
    #     --extras=+q --langmap=c++:.ino.c.h.cpp.hpp \
    #     . ~/.platformio/packages/framework-espidf/ &&
    #
    $pio run -t compiledb &&
    $pio run -t upload &&
    touch $me &&
    monitor
else
    monitor
fi





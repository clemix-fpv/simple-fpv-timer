#!/bin/bash
need_build=${force:-0}
me="./build.sh"
pio=pio
check_folders="src data_src"

if command -v distrobox-enter; then
    pio="distrobox-enter tumbleweed -- pio "
fi

print_help()
{
      echo "Usage:"
      echo "  --force -f     force build"
      echo "  --monitor      Only run monitor"
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
    *)
      echo "Invalid argument"
      echo ""
      print_help
      exit 2;
      ;;
  esac
done


if [ $need_build -eq 1 ]; then
    # universal-ctags -R --exclude='.*.swp' --exclude=.svn \
    #     --exclude=.git --exclude='*~' --c++-kinds=+p --fields=+iaS \
    #     --extras=+q --langmap=c++:.ino.c.h.cpp.hpp \
    #     . ~/.platformio/packages/framework-espidf/ &&
    #
    $pio run -t compiledb &&
    $pio run -t upload &&
    touch $me &&
    $pio device monitor
else
    $pio device monitor
fi





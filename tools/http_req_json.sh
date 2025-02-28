#!/bin/bash

function print_help() {
    echo "Usage:"
    echo "$0 <ip> <path> [<json>]"
    echo ""
    echo "Options:"
    echo " -h|--help            Show this help"
    echo ""
}

POSITIONAL_ARGS=()

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
        print_help;
        exit 0
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

if [[ "${#POSITIONAL_ARGS[@]}" -lt 2 ]]; then
    print_help
    exit 2
fi

url="http://$1/$2"


if [[ "${#POSITIONAL_ARGS[@]}" -eq 2 ]]; then
    curl "$url" -X GET
else
    curl "$url" -X POST -H 'Content-Type: application/json; charset=utf-8'  --data-raw "$3"
fi

#!/bin/sh
DIR=/data
mkdir -p "$DIR"
exec 9>>"$DIR/.lock"
ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' \n')
n=0
current_file=""
cleanup() {
    if [ -n "$current_file" ] && [ -e "$current_file" ]; then
        rm -f "$current_file"
    fi
    exit 0
}
trap cleanup TERM INT
while :; do
    n=$((n + 1))
    flock -x 9
    i=1
    while :; do
        case $i in
            ?)   name=00$i ;;
            ??)  name=0$i  ;;
            *)   name=$i   ;;
        esac
        f=$DIR/$name
        if [ ! -e "$f" ] && ( set -C; printf '%s %s\n' "$ID" "$n" > "$f" ) 2>/dev/null; then
            current_file="$f"
            break
        fi
        i=$((i + 1))
    done
    flock -u 9
    sleep 1
    rm -f "$current_file"
    current_file=""
    sleep 1
done
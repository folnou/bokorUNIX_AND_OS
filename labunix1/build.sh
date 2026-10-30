#!/bin/sh
set -eu

usage() {
    echo "Использование: $0 <исходный_файл>" >&2
    exit 1
}

[ $# -eq 1 ] || usage

srcfile="$1"
[ -f "$srcfile" ] || { echo "Ошибка: Файл '$srcfile' не найден" >&2; exit 2; }
[ -r "$srcfile" ] || { echo "Ошибка: Файл '$srcfile' недоступен для чтения" >&2; exit 2; }

srcbase=$(basename -- "$srcfile")
srcdir=$(dirname -- "$srcfile")

outname=$(grep -m1 -i -o 'Output:[[:space:]]*[^[:space:]]*' -- "$srcfile" | sed 's/^[Oo]utput:[[:space:]]*//' | sed 's/[/*%]*$//' || true)

if [ -z "$outname" ]; then
    echo "Ошибка: Директива 'Output:' не найдена в '$srcfile'" >&2
    exit 3
fi

TMPDIR=$(mktemp -d) || { echo "Ошибка: Не удалось создать временный каталог" >&2; exit 1; }

cleanup() {
    rc=$?
    trap - EXIT HUP INT QUIT PIPE TERM
    rm -rf -- "$TMPDIR"
    exit "$rc"
}
trap cleanup EXIT HUP INT QUIT PIPE TERM

cp -- "$srcfile" "$TMPDIR/"

case "$srcbase" in
    *.c)
        if ! cc -o "$TMPDIR/$outname" "$TMPDIR/$srcbase" 2>"$TMPDIR/build.log"; then
            cat -- "$TMPDIR/build.log" >&2
            exit 5
        fi
        ;;
    *.cpp|*.cc|*.cxx)
        if ! c++ -o "$TMPDIR/$outname" "$TMPDIR/$srcbase" 2>"$TMPDIR/build.log"; then
            cat -- "$TMPDIR/build.log" >&2
            exit 5
        fi
        ;;
    *.tex)
        jobname="${outname%.pdf}"
        if ! pdflatex -interaction=nonstopmode -halt-on-error \
                -output-directory "$TMPDIR" -jobname="$jobname" "$TMPDIR/$srcbase" >"$TMPDIR/build.log" 2>&1; then
            cat -- "$TMPDIR/build.log" >&2
            exit 5
        fi
        if [ ! -s "$TMPDIR/$jobname.pdf" ]; then
            echo "Ошибка: pdflatex отработал, но PDF файл не был создан" >&2
            exit 5
        fi
        if [ "$jobname.pdf" != "$outname" ]; then
            mv -- "$TMPDIR/$jobname.pdf" "$TMPDIR/$outname"
        fi
        ;;
    *)
        echo "Ошибка: Неподдерживаемое расширение файла '$srcbase'" >&2
        exit 4
        ;;
esac

mv -- "$TMPDIR/$outname" "$srcdir/"

echo "Успех: Файл '$outname' собран и сохранен в '$srcdir'"
exit 0
#!/bin/sh
set -eu

usage() {
    echo "Использование: $0 <исходный_файл>" >&2
    exit 1
}

[ $# -eq 1 ] || usage

srcfile="$1"
[ -f "$srcfile" ] || { echo "Ошибка: Файл '$srcfile' не найден" >&2; exit 1; }
[ -r "$srcfile" ] || { echo "Ошибка: Файл '$srcfile' недоступен для чтения" >&2; exit 1; }

srcbase=$(basename -- "$srcfile")
srcdir=$(dirname -- "$srcfile")

outname=$(grep -E -i '^\s*(//|/\*|\*|%|#)\s*.*Output:' -- "$srcfile" | head -n 1 | sed -E 's/.*[Oo]utput:[[:space:]]*//' | awk '{print $1}' | sed 's/[/*%]*$//' || true)

outname=$(basename -- "$outname")
if [ -z "$outname" ] || [ "$outname" = "." ] || [ "$outname" = ".." ]; then
    echo "Ошибка: Директива 'Output:' не найдена или содержит некорректное имя" >&2
    exit 2
fi

BUILD_TMPDIR=$(mktemp -d) || { echo "Ошибка: Не удалось создать временный каталог" >&2; exit 1; }

cleanup() {
    rc=$?
    trap - EXIT HUP INT QUIT PIPE TERM ALRM USR1 USR2
    rm -rf -- "$BUILD_TMPDIR"
    exit "$rc"
}
trap cleanup EXIT HUP INT QUIT PIPE TERM ALRM USR1 USR2

cp -- "$srcfile" "$BUILD_TMPDIR/"

case "$srcbase" in
    *.c)
        if ! cc -o "$BUILD_TMPDIR/$outname" "$BUILD_TMPDIR/$srcbase" 2>"$BUILD_TMPDIR/build.log"; then
            cat -- "$BUILD_TMPDIR/build.log" >&2
            exit 4
        fi
        ;;
    *.cpp|*.cc|*.cxx)
        if ! c++ -o "$BUILD_TMPDIR/$outname" "$BUILD_TMPDIR/$srcbase" 2>"$BUILD_TMPDIR/build.log"; then
            cat -- "$BUILD_TMPDIR/build.log" >&2
            exit 4
        fi
        ;;
    *.tex)
        jobname="${outname%.pdf}"
        if ! pdflatex -interaction=nonstopmode -halt-on-error \
                -output-directory "$BUILD_TMPDIR" -jobname="$jobname" "$BUILD_TMPDIR/$srcbase" >"$BUILD_TMPDIR/build.log" 2>&1; then
            cat -- "$BUILD_TMPDIR/build.log" >&2
            exit 4
        fi
        if [ ! -s "$BUILD_TMPDIR/$jobname.pdf" ]; then
            echo "Ошибка: pdflatex отработал, но PDF файл не был создан" >&2
            exit 4
        fi
        if [ "$jobname.pdf" != "$outname" ]; then
            mv -- "$BUILD_TMPDIR/$jobname.pdf" "$BUILD_TMPDIR/$outname"
        fi
        ;;
    *)
        echo "Ошибка: Неподдерживаемое расширение файла '$srcbase'" >&2
        exit 3
        ;;
esac

mv -- "$BUILD_TMPDIR/$outname" "$srcdir/"

echo "Успех: Файл '$outname' собран и сохранен рядом с исходником"
exit 0
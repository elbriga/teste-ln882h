#!/bin/bash

echo "platformio.ini"
echo "==="
cat platformio.ini
echo

for ARQ in `ls include/`; do
    if [ -d "include/$ARQ" ]; then
        [ "$ARQ" == "images" ] && continue
        # 1 nivel de recursao manual
        for ARQ2 in `ls include/$ARQ/`; do
            echo "$ARQ/$ARQ2"
            echo "==="
            cat "include/$ARQ/$ARQ2"
            echo
        done
    else
        echo "include/$ARQ"
        echo "==="
        cat "include/$ARQ"
        echo
    fi
done

for ARQ in `ls src/`; do
    if [ -d "src/$ARQ" ]; then
        # 1 nivel de recursao manual
        for ARQ2 in `ls src/$ARQ/`; do
            echo "$ARQ/$ARQ2"
            echo "==="
            cat "src/$ARQ/$ARQ2"
            echo
        done
    else
        echo $ARQ
        echo "==="
        cat "src/$ARQ"
    fi
    echo
done

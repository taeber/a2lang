#!/bin/bash

cd $(dirname $0)

mkdir -p output

for each in `ls -1 *.a2`
do
    ../compile "$each" 2>output/"$each".err >output/"$each".out
    if [ $? -eq 0 ]
    then
        echo " ✅  $each"
    else
        echo " ❌  $each"
    fi
done

# Try examples of unsupported syntax
for each in `ls -1 *.a2e`
do
    ../compile "$each" 2>output/"$each".err >output/"$each".out
    if [ $? -eq 0 ]
    then
        echo " ❌  $each"
    else
        echo " ✅  $each"
    fi
done

if [ -f ../tester ]
then
    pushd ..
    echo Running some tests using fake6502
    for each in `grep --files-with-matches FAIL tests/*.a2`
    do
        ./a2 build "$each" 2>&1 >/dev/null
        ./tester OUT.6502 --quiet | grep --quiet FAIL
        if [ $? -eq 0 ]
        then
            echo " ❌  $each"
        else
            echo " ✅  $each"
        fi
    done
    popd
fi

#!/bin/bash

cd $(dirname $0)

mkdir -p output

for each in `ls -1 *.a2`
do
    ../src/compile "$each" 2>output/"$each".err >output/"$each".out
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
    ../src/compile "$each" 2>output/"$each".err >output/"$each".out
    if [ $? -eq 0 ]
    then
        echo " ❌  $each"
    else
        echo " ✅  $each"
    fi
done


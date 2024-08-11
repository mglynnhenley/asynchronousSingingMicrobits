#!/bin/bash

declare -i numubits=0

for i in {0..10}
do
  if test -f "../tools/note$i.cpp"; then
    numubits=i+1
  else
    break
  fi
done

echo "found $numubits note files"

rm MICROBIT.hex
cmake -B cmake-build-file .

for (( i=0; i<numubits; i++ ))
do
  python3 replace.py $i

  cmake --build cmake-build-file --target MICROBIT_hex "-j6"

  md5 MICROBIT.hex
  mv MICROBIT.hex MICROBIT0.hex
  cp MICROBIT0.hex /Volumes/MICROBIT/MICROBIT.hex
done

cp MICROBIT0.hex /Volumes/MICROBIT/MICROBIT.hex
cp MICROBIT1.hex /Volumes/MICROBIT\ 1/MICROBIT.hex
cp MICROBIT2.hex /Volumes/MICROBIT\ 2/MICROBIT.hex

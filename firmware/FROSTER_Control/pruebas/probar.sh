#!/bin/bash
# Extrae del sketch la logica de control y las senales (LED y buzzer) y las prueba en la PC.
cd "$(dirname "$0")"
INO=../FROSTER_Control.ino
sed -n '/>>> LOGICA_INICIO/,/<<< LOGICA_FIN/p' $INO > logica_extraida.h
awk '/SENALES: LED RGB y buzzer pasivo/{f=1} /ARRANQUE Y BUCLE/{f=0} f' $INO | sed '1s/^/\/* /' | sed '$d' > senales_extraidas.h
g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -fsanitize=undefined,address \
    probar_logica.cpp -o probar_logica 2>&1 | grep -E "error" | head -20
g++ -std=c++17 -Wall -Wno-unused-function probar_senales.cpp -o probar_senales 2>&1 | grep -E "error" | head -20
g++ -std=c++17 -Wall -Wno-unused-function -DBUZZER_ACTIVO_BAJO=1 probar_senales.cpp -o probar_senales_bajo 2>&1 | grep -E "error" | head -20
./probar_logica; r1=$?
./probar_senales; r2=$?
./probar_senales_bajo; r3=$?
rm -f probar_logica probar_senales probar_senales_bajo logica_extraida.h senales_extraidas.h
exit $(( r1 || r2 || r3 ))

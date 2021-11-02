#!/bin/sh
rm ./simu1/full
rm ./simu1/mixed
rm ./simu1/my-judge

rm ./simu2/full
rm ./simu2/mixed
rm ./simu2/my-judge

rm ./simu3/full
rm ./simu3/mixed
rm ./simu3/my-judge

rm ./simu4/full
rm ./simu4/mixed
rm ./simu4/my-judge

cd simu1
g++ -o full full.cpp -O2 -Wall -Wextra -Wshadow
g++ -o mixed mixed.cpp -O2 -Wall -Wextra -Wshadow
g++ -o my-judge my-judge.cpp -O2 -Wall -Wextra -Wshadow

cd ..

cp ./simu1/full ./simu2
cp ./simu1/mixed ./simu2
cp ./simu1/my-judge ./simu2

cp ./simu1/full ./simu3
cp ./simu1/mixed ./simu3
cp ./simu1/my-judge ./simu3

cp ./simu1/full ./simu4
cp ./simu1/mixed ./simu4
cp ./simu1/my-judge ./simu4

g++ -o optimize optimize.cpp -O2 -Wall -Wextra -Wshadow
g++ -o launch-games launch-games.cpp -O2 -Wall -Wextra -Wshadow
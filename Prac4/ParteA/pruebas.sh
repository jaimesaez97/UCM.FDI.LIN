#!/bin/bash

echo 'Inserta parametro: 1 borrar, 0 añadir'
read VAR

if [ $VAR -eq 0 ]; then
	for i in 0 1 2 3 4 5 6 7 8 9 10 
	do
		echo 'add' $i > /proc/modlist 
		echo 'tick' $i
		sleep 1
	done
else
	for i in 0 1 2 3 4 5 6 7 8 9 10
	do
		echo 'remove' $i > /proc/modlist
		echo 'tick' $i
		sleep 1
	done
fi

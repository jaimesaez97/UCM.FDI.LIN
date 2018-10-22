echo "-----------------------------------\n"
echo "Comenzando SCRIPT DE COMPROBACION..."
echo "------------------------------------\n"
echo "Compilamos la parte opcional 1 (EXTRA_CFLAGS=-DPARTE_OPCIONAL make)"
sleep 2
EXTRA_CFLAGS=-DPARTE_OPCIONAL make
echo "------------------------------------\n"
echo "Instanciamos el modulo"
echo "sudo insmod mi_modulo_opc.ko"
sudo insmod mi_modulo_opc.ko
echo "------------------------------------\n"
echo "Anyadimos numeros a la lista"
sleep 2
echo "echo add 1 > /proc/modlist"
echo add 1 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 2 > /proc/modlist"
echo add 2 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 3 > /proc/modlist"
echo add 3 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 4 > /proc/modlist"
echo add 4 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 5 > /proc/modlist"
echo add 5 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 6 > /proc/modlist"
echo add 6 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 7 > /proc/modlist"
echo add 7 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 8 > /proc/modlist"
echo add 8 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 9 > /proc/modlist"
echo add 9 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 9 > /proc/modlist"
echo add 9 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 9 > /proc/modlist"
echo add 9 > /proc/modlist
echo "------------------------------------\n"
echo "echo add 8 > /proc/modlist"
echo add 8 > /proc/modlist
sleep 1
echo "------------------------------------\n"
echo "hemos añadido numeros del [1..9] ++ [9,9,8]"
sleep 1
echo "------------------------------------\n"
echo "cat /proc/modlist"
echo "Mostramos la lista:"
cat /proc/modlist
sleep 3
echo "------------------------------------\n"
echo "Borramos el 9"
echo "echo remove 9 > /proc/modlist"
echo remove 9 > /proc/modlist
sleep 1
echo "------------------------------------\n"
echo "cat /proc/modlist"
cat /proc/modlist
sleep 2
echo "------------------------------------\n"
echo "echo remove 1 > /proc/modlist"
echo remove 1 > /proc/modlist
sleep 1
echo "------------------------------------\n"
echo "cat /proc/modlist"
cat /proc/modlist
sleep 2
echo "------------------------------------\n"
echo "echo cleanup > /proc/modlist"
echo cleanup > /proc/modlist
sleep 1
echo "------------------------------------\n"
echo "cat /proc/modlist"
cat /proc/modlist
echo "<lista_vacia>"
sleep 2
echo "------------------------------------\n"
echo "Eliminando modulo..."
sleep 1
echo "------------------------------------\n"
echo "sudo rmmod mi_modulo_opc.ko"
sudo rmmod mi_modulo_opc.ko
echo "------------------------------------\n"
echo "make clean\n"
make clean

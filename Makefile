stress_client : main.o
	gcc main.o -o stress_client 

main.o : main.c
	gcc -c main.c -o main.o 
clean :
	rm stress_client main.o

all: bank

bank: bank.c string_parser.c
	gcc -g -W -Wall -pthread -lpthread -o bank bank.c string_parser.c

clean:
	rm ./bank ./output/*.txt ./savings/*.txt output.txt
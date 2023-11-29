all: bank

bank: bank.c string_parser.c
	gcc -g -W -Wall -pthread -o bank bank.c string_parser.c

clean:
	rm ./bank ./output/*.txt ./savings/*.txt output.txt
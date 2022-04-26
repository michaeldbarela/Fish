swim_mill: swim_mill.c fish pellet
	gcc -pthread -o swim_mill swim_mill.c

fish: fish.c
	gcc -o fish fish.c

pellet: pellet.c
	gcc -o pellet pellet.c

clean:
	rm swim_mill fish pellet *.txt

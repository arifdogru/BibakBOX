all:
	gcc -c -std=gnu99 Server.c
	gcc -c -std=gnu99 Client.c
	gcc -o BibakBOXServer Server.o -lpthread
	gcc -o BibakBOXClient Client.o -lpthread
	@echo "Usage: ./BibakBOXServer [directory] [threadPool Size] [port]"
	@echo "Usage: ./BibakBOXClient [directory] [ip address] [port]"
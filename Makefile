all: server subscriber

common.o: common.c

server: server.c common.o

subscriber: client.c common.o
	$(CC) -o $@ $^

.PHONY: clean run_server run_client


run_server:
	./server ${IP_SERVER} ${PORT}
   
run_client:
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM


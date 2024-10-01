all: clean
	gcc server.c -D WRITE_SERVER -o write_server
	gcc server.c -D READ_SERVER -o read_server

binaries=write_server read_server
.PHONY: clean
clean:
	rm -f $(binaries) *.o
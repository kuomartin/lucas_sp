all: clean
	gcc server.c -D WRITE_SERVER -o write_server
	gcc server.c -D READ_SERVER -o read_server

binaries=write_server read_server
.PHONY: clean
clean:
	rm -f $(binaries) *.o
 
run_servers: all
	nohup ./write_server 9034 > write_server.log 2>&1 &
	nohup ./read_server 9033 > read_server.log 2>&1 &
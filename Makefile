.PHONY: all clean
all:
	gcc server.c -D WRITE_SERVER $(defines) -o write_server
	gcc server.c -D READ_SERVER $(defines) -o read_server

binaries=write_server read_server
runner=nohup
write_port=9034
read_port=9033

clean:
	rm -f $(binaries) *.o

run_servers:
	$(runner) ./write_server 9034 > write_server.log 2>&1 &
	$(runner) ./read_server 9033 > read_server.log 2>&1 &

test: defines = -D FAST
test: runner = 
test: all run_servers
	@echo run write_server on port $(write_port)
	@echo run read_server on port $(read_port)
	
kill:
	@kill -9 `ps -aux | grep write_server | grep -v grep | awk '{print $$2}'` > /dev/null 2>&1 || true
	@kill -9 `ps -aux | grep read_server | grep -v grep | awk '{print $$2}'` > /dev/null 2>&1 || true
	

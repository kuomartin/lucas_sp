all: clean
	gcc server.c -D WRITE_SERVER -o write_server
	gcc server.c -D READ_SERVER -o read_server

binaries=write_server read_server
.PHONY: clean
clean:
	rm -f $(binaries) *.o

run_servers: all
	nohup ./write_server 9034 > write_server.log 2>&1 &
	echo $$! >> nohup_pids.txt
	nohup ./read_server 9033 > write_server.log 2>&1 &
	echo $$! >> nohup_pids.txt
kill:
	if[-f nohup_pids.txt ]; then
		cat nohup_pids.txt | while read line; do kill -9 $$line ; done
		rm nohup_pids.txt
	fi
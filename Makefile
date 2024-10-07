.PHONY: all clean run-server test kill

NC='\033[0m'
Black='\033[0;30m'
Red='\033[0;31m'
Green='\033[0;32m'
Yellow='\033[0;33m'
Blue='\033[0;34m'
Purple='\033[0;35m'
Cyan='\033[0;36m'
White='\033[0;37m' 

binaries=write_server read_server
conn=nc

runner?=nohup
wp?=9034
rp?=9033

all: write_server write_server

write_server:
	gcc server.c -D WRITE_SERVER $(defines) -o write_server
read_server:
	gcc server.c -D READ_SERVER $(defines) -o read_server

clean:
	rm -f $(binaries) *.o *.log

run-server: write_server read_server
	@echo  -e $(Purple)run server:$(NC)
	$(runner) ./write_server $(wp) > write_server.log 2>&1 &
	$(runner) ./read_server $(rp) > read_server.log 2>&1 &
	@echo
	@echo \'$(conn) localhost $(wp)\' to connect write_server
	@echo \'$(conn) localhost $(rp)\' to connect read_erver
	@echo

test: defines=-D FAST
test: runner=
test: kill run-server

kill: wpids:=$(shell ps -aux | grep write_server | grep -v grep | awk '{print $$2}')
kill: rpids:=$(shell ps -aux | grep read_server | grep -v grep | awk '{print $$2}')
kill:
	@echo -e $(Red)kill:$(NC)
	@if ! [ "$(rpids)" ] && ! [ "$(rpids)" ] ; then \
		echo nothing to kill! ;\
	else \
		if [ "$(wpids)" ] ;then (kill -9 $(wpids)); echo killed write_server: $(wpids) ;fi ;\
		if [ "$(rpids)" ] ;then	(kill -9 $(rpids)); echo killed read_server: $(rpids) ; fi ;\
	fi
	@echo

.PHONY: all clean run run-nohup test kill

# If the first argument is "test"...
ifeq (test,$(firstword $(MAKECMDGOALS)))
	test=true
endif

ifdef test
	defs = -D TEST
endif

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

rootdir = $(realpath .)

wp?=9034
rp?=9033
UC = $(shell echo '$1' | tr '[:lower:]' '[:upper:]')

all:| clean write_server read_server

%_server:
	gcc server.c -D $(call UC,$*)_SERVER $(defs) -o $*_server
clean:
	$(eval useless:=$(shell ls $(binaries) *.o *.log 2>/dev/null))
	@$(if $(useless),,echo nothing to clean!)
	@$(foreach x,$(useless), echo remove ./$(x) ; rm $(x);)

run: write_server read_server
	@echo  -e $(Purple)run server:$(NC)
	$(runner) ./write_server $(wp) > write_server.log 2>&1 &
	$(runner) ./read_server $(rp) > read_server.log 2>&1 &
	@echo 
	@echo Use \'$(MAKE) run-nohup\' to run nohup server.
	@echo
	@echo \'$(conn) localhost $(wp)\' to connect write_server.
	@echo \'$(conn) localhost $(rp)\' to connect read_erver.

run-nohup: runner=nohup
run-nohup: run

kill: wpids:=$(shell ps -aux | grep write_server | grep -v grep | awk '{print $$2}')
kill: rpids:=$(shell ps -aux | grep read_server | grep -v grep | awk '{print $$2}')
kill:
	@echo -e $(Red)kill:$(NC)
	$(if $(and $(rpids),$(wpids)),,@echo nothing to kill!)
	$(if $(rpids),kill -9 $(rpids))
	$(if $(wpids),kill -9 $(wpids))
	@echo
test:
	@echo -e $(Blue)test: remove timeout$(NC)
ifeq ($(words $(MAKECMDGOALS)),1)
	@$(MAKE) test=true all
endif
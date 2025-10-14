
CC = gcc
CFLAGS = -Wall -Wno-implicit-function-declaration

all: sim_mon_srv mon_cli

COMMON_SRC = libcup_dump.c mavlink_print.c mon_print.c

SRV_SRC = $(COMMON_SRC) sim_mon_srv.c
sim_mon_srv: dptm_tls_mon.h mon_print.h $(SRV_SRC)
	$(CC) $(CFLAGS) -o $@ $(SRV_SRC)

CLI_SRC = $(COMMON_SRC) mon_print.h mon_cli.c
mon_cli: dptm_tls_mon.h $(CLI_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLI_SRC)

.PHONY: clean
clean:
	rm -f *.o sim_mon_srv mon_cli

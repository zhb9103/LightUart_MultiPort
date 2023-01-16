all: clean compile

compile:
	vellib work
	velmap work work
	velanalyze LightUartTransactor.sv DceLoopback.sv Top.sv
	velcomp -top Top 


build: uart-xterm
	velhvl -sim veloce -64bit_runtime -cfiles ${TBX_HOME}/include/tbx_main.cxx dpi.c


run:
	velrun -c -do "run.do"


clean:
	rm -rf work *dir *log  *map *med *map transcript modelsim.ini veloce.wave


uart-xterm: uart-xterm.c
	${GCC_EXE} -Wall -g -o $@ $< -lpthread

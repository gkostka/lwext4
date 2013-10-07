
all: generic bf518 cortex-m3 cortex-m4 generic

bf518:
	rm -R -f bf518
	mkdir bf518
	cd bf518 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../src/toolchain/bf518.cmake ../src
	cd bf518 && make

cortex-m3:
	rm -R -f cortex-m3
	mkdir cortex-m3
	cd cortex-m3 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../src/toolchain/cortex-m3.cmake ../src
	cd cortex-m3 && make

cortex-m4:
	rm -R -f cortex-m4
	mkdir cortex-m4
	cd cortex-m4 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../src/toolchain/cortex-m4.cmake ../src
	cd cortex-m4 && make
	
	
generic:
	rm -R -f generic
	mkdir generic
	cd generic && cmake -G"Unix Makefiles" ../src
	cd generic&& make
	
	
clean:
	rm -R -f bf518
	rm -R -f cortex-m3
	rm -R -f cortex-m4
	rm -R -f generic

	
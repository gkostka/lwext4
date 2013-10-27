
generic:
	rm -R -f build_generic
	mkdir build_generic
	cd build_generic && cmake -G"Unix Makefiles" ../
	cd build_generic && make
	
bf518:
	rm -R -f build_bf518
	mkdir build_bf518
	cd build_bf518 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain/bf518.cmake ..
	cd build_bf518 && make

cortex-m3:
	rm -R -f build_cortex-m3
	mkdir build_cortex-m3
	cd build_cortex-m3 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m3.cmake ..
	cd build_cortex-m3 && make

cortex-m4:
	rm -R -f build_cortex-m4
	mkdir build_cortex-m4
	cd build_cortex-m4 && cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m4.cmake ..
	cd build_cortex-m4 && make

all: generic bf518 cortex-m3 cortex-m4 generic


	
clean:
	rm -R -f build_bf518
	rm -R -f build_cortex-m3
	rm -R -f build_cortex-m4
	rm -R -f build_generic

	

#Release
#Debug
BUILD_TYPE = Debug

#Check: http://www.cmake.org/Wiki/CMake_Generator_Specific_Information
#"Unix Makefiles"
#"Eclipse CDT4 - Unix Makefiles"
PROJECT_SETUP = "Eclipse CDT4 - Unix Makefiles"

generic:
	rm -R -f build_generic
	mkdir build_generic
	cd build_generic && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..
	
bf518:
	rm -R -f build_bf518
	mkdir build_bf518
	cd build_bf518 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/bf518.cmake ..

cortex-m3:
	rm -R -f build_cortex-m3
	mkdir build_cortex-m3
	cd build_cortex-m3 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m3.cmake ..
	

cortex-m4:
	rm -R -f build_cortex-m4
	mkdir build_cortex-m4
	cd build_cortex-m4 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m4.cmake ..
	
all: generic bf518 cortex-m3 cortex-m4 generic


clean:
	rm -R -f build_bf518
	rm -R -f build_cortex-m3
	rm -R -f build_cortex-m4
	rm -R -f build_generic

	
include fs_test.mk


	

	
	
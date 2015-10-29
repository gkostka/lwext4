
#Release
#Debug
BUILD_TYPE = Release

#Check: http://www.cmake.org/Wiki/CMake_Generator_Specific_Information
#"Unix Makefiles"
#"Eclipse CDT4 - Unix Makefiles"
PROJECT_SETUP = "Unix Makefiles"

generic:
	rm -R -f build_generic
	mkdir build_generic
	cd build_generic && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/generic.cmake ..
	
bf518:
	rm -R -f build_bf518
	mkdir build_bf518
	cd build_bf518 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/bf518.cmake ..

avrxmega7:
	rm -R -f build_avrxmega7
	mkdir build_avrxmega7
	cd build_avrxmega7 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/avrxmega7.cmake ..

msp430:
	rm -R -f build_msp430
	mkdir build_msp430
	cd build_msp430 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/msp430.cmake ..


cortex-m0:
	rm -R -f build_cortex-m0
	mkdir build_cortex-m0
	cd build_cortex-m0 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m0.cmake ..
	
cortex-m3:
	rm -R -f build_cortex-m3
	mkdir build_cortex-m3
	cd build_cortex-m3 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m3.cmake ..
	
cortex-m4:
	rm -R -f build_cortex-m4
	mkdir build_cortex-m4
	cd build_cortex-m4 && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m4.cmake ..

arm-sim:
	rm -R -f build_arm-sim
	mkdir build_arm-sim
	cd build_arm-sim && cmake -G$(PROJECT_SETUP) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-sim.cmake ..
	
all: generic bf518 cortex-m3 cortex-m4 generic


clean:
	rm -R -f build_*
	rm -R -f ext_images
	
unpack_images:
	rm -R -f ext_images
	7z x ext_images.7z

	
include fs_test.mk


	

	
	


#Release
#Debug
BUILD_TYPE = Release

ifneq ($(shell test -d .git), 0)
GIT_SHORT_HASH:= $(shell git rev-parse --short HEAD)
endif

VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION_PATCH = 0

VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)-$(GIT_SHORT_HASH)

COMMON_DEFINITIONS =                                      \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)                      \
	-DVERSION_MAJOR=$(VERSION_MAJOR)                      \
	-DVERSION_MINOR=$(VERSION_MINOR)                      \
	-DVERSION_PATCH=$(VERSION_PATCH)                      \
	-DVERSION=$(VERSION)                                  \


generic:
	rm -R -f build_generic
	mkdir build_generic
	cd build_generic && cmake -G"Unix Makefiles"          \
	$(COMMON_DEFINITIONS)                                 \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/generic.cmake ..


avrxmega7:
	rm -R -f build_avrxmega7
	mkdir build_avrxmega7
	cd build_avrxmega7 && cmake -G"Unix Makefiles"        \
	$(COMMON_DEFINITIONS)                                 \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/avrxmega7.cmake ..

msp430:
	rm -R -f build_msp430
	mkdir build_msp430
	cd build_msp430 && cmake -G"Unix Makefiles"           \
	$(COMMON_DEFINITIONS)                                 \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/msp430.cmake ..


cortex-m0:
	rm -R -f build_cortex-m0
	mkdir build_cortex-m0
	cd build_cortex-m0 && cmake -G"Unix Makefiles"       \
	$(COMMON_DEFINITIONS)                                \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m0.cmake ..
	
cortex-m3:
	rm -R -f build_cortex-m3
	mkdir build_cortex-m3
	cd build_cortex-m3 && cmake -G"Unix Makefiles"       \
	$(COMMON_DEFINITIONS)                                \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m3.cmake ..
	
cortex-m4:
	rm -R -f build_cortex-m4
	mkdir build_cortex-m4
	cd build_cortex-m4 && cmake -G"Unix Makefiles"       \
	$(COMMON_DEFINITIONS)                                \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/cortex-m4.cmake ..

arm-sim:
	rm -R -f build_arm-sim
	mkdir build_arm-sim
	cd build_arm-sim && cmake -G"Unix Makefiles"         \
	$(COMMON_DEFINITIONS)                                \
	-DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-sim.cmake ..

lib_only:
	rm -R -f build_lib_only
	mkdir build_lib_only
	cd build_lib_only && cmake $(COMMON_DEFINITIONS) -DLIB_ONLY=TRUE ..

all: generic bf518 cortex-m3 cortex-m4 lib_only


clean:
	rm -R -f build_*
	rm -R -f ext_images
	
unpack_images:
	rm -R -f ext_images
	7z x ext_images.7z

	
include fs_test.mk


	

	
	

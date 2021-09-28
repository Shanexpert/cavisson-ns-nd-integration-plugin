include ../../../../../.cav.build 
SHELL=/bin/bash
CFLAG_FILE=.cflags.mk

#Get Distro 
DISTRO = $(__distro)
RELEASE = $(__release) 
IsRelGE = $(shell if [[ $(DISTRO) == $(1) && $(RELEASE) -ge $(2) ]];then echo 1; else echo 0; fi)
IsRelEQ = $(shell if [[ $(DISTRO) == $(1) && $(RELEASE) -eq $(2) ]];then echo 1; else echo 0; fi)

#Get Fedora os bit 32/64
ARCH_BITS = $(__arch_bits)
NS_RELEASE = $(__ns_release)

#Set library path basis of 32 or 64
ifeq ($(ARCH_BITS),64)
  LIB_PATH=/usr/lib64
else
  LIB_PATH=/usr/lib
endif

LDFLAGS = -g -m${ARCH_BITS}

#Lib
ifeq ($(call IsRelGE,"Fedora",14), 1)
   CFLAG += -Wall ${LDFLAGS} -c -DRELEASE=$(RELEASE) -D$(DISTRO) -DENABLE_JIFFY_TS 
else ifeq ($(call IsRelGE,"Ubuntu",1604), 1)
   INCLUDE += -I/usr/include/postgresql/ 
   CFLAG += -fgnu89-inline $(INCLUDE) -Wall ${LDFLAGS} -c -DRELEASE=$(RELEASE) -D$(DISTRO) -DENABLE_JIFFY_TS -g 
else ifeq ($(call IsRelGE,"Ubuntu",1204), 1)
   INCLUDE += -I/usr/include/postgresql/ 
   CFLAG += $(INCLUDE) -Wall ${LDFLAGS} -c -DRELEASE=$(RELEASE) -D$(DISTRO) -DENABLE_JIFFY_TS -g
else ifeq ($(call IsRelEQ,"Fedora",9), 1)
   CFLAG += $(INCLUDE) -Wall ${LDFLAGS} -c -DRELEASE=$(RELEASE) -D$(DISTRO) -DENABLE_JIFFY_TS 
   LIB_DB=$(LIB_PATH)/libdb-4.6.a
else ifeq ($(call IsRelEQ,"Fedora",4), 1)
   CFLAG += $(INCLUDE) -Wall ${LDFLAGS} -c -DRELEASE=$(RELEASE) -D$(DISTRO)
else
   $(error "CFLAG could not set as distro and version couldnt be matched")
endif

#NetStorm Core Library
LIB_NS_CORE_OBJ_DIR=$(__lib_dir)
LIB_ARC=libnscore.a

export DISTRO
export RELEASE 
export ARCH_BITS 
export LIB_PATH
export INCLUDE 
export NS_LIBS
export LIBS 
export CFLAG_FILE
export NS_RELEASE
export LDFLAGS

all: clean build release do_export

clean: 
	>${CFLAG_FILE}
	make -f Makefile.common clean

build:
	echo -n >${CFLAG_FILE}
	echo OBJ_DIR = cdr_obj >> ${CFLAG_FILE}
	echo LIBS +=  ${LIB_NS_CORE_OBJ_DIR}/${LIB_ARC} -L${LIB_PATH} -lm -lpq -lz -lpthread -lcurl -ldl -lcrypto >> ${CFLAG_FILE}
	make -f Makefile.common build

release:
	make -f Makefile.common release

release_bin:
	make -f Makefile.common release_bin

release_rhel_bin:
	make -f Makefile.common release_rhel_bin

do_export:
	make -f Makefile.common do_export

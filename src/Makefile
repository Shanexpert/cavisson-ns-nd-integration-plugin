
include ../../../../.cav.build
export LD_LIBRARY_PATH=$(__lib_dir)

CFLAGMK = .cflags.mk

BUILD_USER=$(shell whoami)
BUILD_LOG="build/.build.log"


#Check gdf id's for duplicate
OUTPUT = $(shell ./check_duplicate_ids)
ifeq ($(OUTPUT), 0)
  $(info "No duplicate group id's found. Hence Continuing . . . .")
else
  #Error: if group id's are duplicate.
  $(error "One or more Group id of GDF are same. $(OUTPUT). Please fix above issue. There is a shell 'show_group_ids.sh' inside gdf directory, which lists the group id which has been used. You can choose the next group id after the last mentioned id")
endif
             
all: clean build build_cavgen release 

clean:
	>${CFLAGMK} 
	make -f Makefile.common clean

build: build_debug build_non_debug

build_cavgen: cavgen_debug cavgen_non_debug

build_debug:
	echo "BUILD_MODE=debug" >${CFLAGMK}
	echo OBJ_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo BIN_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo LIB_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo NS_BIN = netstorm.debug >> ${CFLAGMK} 
	echo NS_DB_UPLOAD_BIN_NAME = nsu_db_upload_dbg >> ${CFLAGMK} 
	echo NS_STRING_API_SO_NAME = ns_string_api_debug.so >> ${CFLAGMK} 
	echo ALL_LIBS = $(__all_debug_libs) >> ${CFLAGMK} 
	echo PROD_LIBS = -L$(__lib_dir) $(__nstopo_v1_debug_libs) $(__nscore_debug_libs) $(__syslib_dir) -lm -lodbc -lxml2_cav >> ${CFLAGMK}
	echo ND_AGGREGATOR_SO_NAME = nd_aggregator_api_debug.so >> ${CFLAGMK}
	echo NS_RELEASE = $(__ns_release) >> ${CFLAGMK}
	
	if [[ "X$(BUILD_USER)" = "Xbuild" ]];then \
		make -f Makefile.common build; \
	else \
		mkdir -p build; \
		> $(BUILD_LOG); \
		make -f Makefile.common build 2>$(BUILD_LOG); \
		$(__tool_dir)/get_proj_warns 1 "NetStorm debug "; \
	fi

build_non_debug:
	echo "BUILD_MODE=non_debug" >${CFLAGMK}
	echo NS_BIN = netstorm >> ${CFLAGMK} 
	echo NS_DB_UPLOAD_BIN_NAME = nsu_db_upload >> ${CFLAGMK} 
	echo NS_STRING_API_SO_NAME = ns_string_api.so >> ${CFLAGMK} 
	echo ALL_LIBS = $(__all_libs) >> ${CFLAGMK}
	echo PROD_LIBS = -L$(__lib_dir) $(__nstopo_v1_libs) $(__nscore_libs) $(__syslib_dir) -lm -lodbc -lxml2_cav >> ${CFLAGMK}
	echo ND_AGGREGATOR_SO_NAME = nd_aggregator_api.so >> ${CFLAGMK}
	echo NS_RELEASE = $(__ns_release) >> ${CFLAGMK}
	
	if [[ "X$(BUILD_USER)" = "Xbuild" ]];then \
		make -f Makefile.common build; \
	else \
		mkdir -p build; \
		> $(BUILD_LOG); \
		make -f Makefile.common build 2>$(BUILD_LOG); \
		$(__tool_dir)/get_proj_warns 1 "NetStorm non-debug "; \
	fi

cavgen_debug:
	echo "BUILD_MODE=cavgen_debug" >${CFLAGMK}
	echo OBJ_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo OBJ_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo BIN_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo BIN_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo LIB_CFLAGS += -DNS_DEBUG_ON -DDB_DEBUG_ON >> ${CFLAGMK}
	echo LIB_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo NS_BIN = cavgen.debug >> ${CFLAGMK}
	echo NS_DB_UPLOAD_BIN_NAME = nsu_db_upload_dbg >> ${CFLAGMK} 
	echo NS_STRING_API_SO_NAME = cg_string_api_debug.so >> ${CFLAGMK} 
	echo ALL_LIBS = $(__all_cgdebug_libs) >> ${CFLAGMK} 
	echo PROD_LIBS = -L$(__lib_dir) $(__nstopo_v1_debug_libs) $(__cgcore_debug_libs) $(__syslib_dir) -lm -lodbc -lxml2_cav >> ${CFLAGMK}
	echo ND_AGGREGATOR_SO_NAME = nd_aggregator_api_debug.so >> ${CFLAGMK}
	echo NS_RELEASE = $(__ns_release) >> ${CFLAGMK}
	
	if [[ "X$(BUILD_USER)" = "Xbuild" ]];then \
		make -f Makefile.common build; \
	else \
		mkdir -p build; \
		> $(BUILD_LOG); \
		make -f Makefile.common build 2>$(BUILD_LOG); \
		$(__tool_dir)/get_proj_warns 1 "Cavgen Debug "; \
	fi

cavgen_non_debug:
	echo "BUILD_MODE=cavgen_non_debug" >${CFLAGMK}
	echo OBJ_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo BIN_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo LIB_CFLAGS += -DCAV_MAIN >> ${CFLAGMK}
	echo NS_BIN = cavgen >> ${CFLAGMK}
	echo NS_DB_UPLOAD_BIN_NAME = nsu_db_upload >> ${CFLAGMK} 
	echo NS_STRING_API_SO_NAME = cg_string_api.so >> ${CFLAGMK} 
	echo ALL_LIBS = $(__all_cglibs) >> ${CFLAGMK}
	echo PROD_LIBS = -L$(__lib_dir) $(__nstopo_v1_libs) $(__cgcore_libs) $(__syslib_dir) -lm -lodbc -lxml2_cav >> ${CFLAGMK}
	echo ND_AGGREGATOR_SO_NAME = nd_aggregator_api.so >> ${CFLAGMK}
	echo NS_RELEASE = $(__ns_release) >> ${CFLAGMK}
	
	if [[ "X$(BUILD_USER)" = "Xbuild" ]];then \
		make -f Makefile.common build; \
	else \
		mkdir -p build; \
		> $(BUILD_LOG); \
		make -f Makefile.common build 2>$(BUILD_LOG); \
		$(__tool_dir)/get_proj_warns 1 "Cavgen Non-Debug "; \
	fi

release_ubuntu: 
	sed -i "s/NS_RELEASE =.*/NS_RELEASE = $(__ns_release)/g" ${CFLAGMK} >/dev/null
	$(__tool_dir)/get_proj_warns 2 "NetStorm Ubuntu "
	make -f Makefile.common release_bin
	$(__tool_dir)/get_proj_warns 3 "NetStorm Ubuntu "

release_rhel:
	sed -i "s/NS_RELEASE =.*/NS_RELEASE = Redhat_64/g" ${CFLAGMK} >/dev/null
	$(__tool_dir)/get_proj_warns 2 "NetStorm RadHat "
	make -f Makefile.common release_bin
	$(__tool_dir)/get_proj_warns 3 "NetStorm RadHat "

increas_build:
	make -f Makefile.common increas_build

release_bin:
	make -f Makefile.common release
	make release_ubuntu
	make release_rhel

release: increas_build release_bin

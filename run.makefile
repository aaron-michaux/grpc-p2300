
# These will be set from the outside
TARGET?=grpc-p2300
VERBOSE?=False

# Affects build environment variables; @see build-env.sh
TOOLCHAIN?=gcc
BUILD_CONFIG?=release
CXXSTD?=-std=c++2b
UNITY_BUILD?=False
BUILD_TESTS?=False
BUILD_EXAMPLES?=False
BENCHMARK?=False
COVERAGE?=False
STDLIB?=stdcxx
LTO?=False
GEN_DIR?=build/generated

# ----------------------------------------------------------------------------- Set base build flags

INCDIRS:=-Isrc -isystem$(GEN_DIR)
BOOST_DEFINES:=-DBOOST_NO_TYPEID -DBOOST_ERROR_CODE_HEADER_ONLY -DBOOST_ASIO_SEPARATE_COMPILATION -DBOOST_ASIO_NO_DEPRECATED -DBOOST_ASIO_DISABLE_VISIBILITY
DEFINES:=$(BOOST_DEFINES) -DUSE_ASIO -DFMT_HEADER_ONLY
WARNINGS:=-Wno-variadic-macros

GRPC_LIBS:=-lgrpc++ -lgrpc -lgpr -lupb -laddress_sorting -lprotobuf -lre2 -lcares -labsl  
SYS_LIBS:=-lssl -lcrypto -lz -lpthread

CFLAGS:=$(INCDIRS) $(DEFINES) $(WARNINGS)
CPPFLAGS:=
CXXFLAGS:=$(INCDIRS) $(DEFINES) $(WARNINGS)
LDFLAGS:=
LIBS:=-lunifex $(GRPC_LIBS) $(SYS_LIBS)

# --------------------------------------------------------------------------- Add Source Directories

PROTOS:=protos/helloworld.proto
GRPC_PROTOS:=protos/helloworld.proto

# Means that this target must be built
DEP_LIBS:=libabsl.a

SOURCES:=$(shell find src -type f -name '*.cpp' -o -name '*.cc' -o -name '*.c') 

# Add in the proto/grpc sources
SOURCES+=$(patsubst %.proto, $(GEN_DIR)/%.pb.cc, $(PROTOS))
SOURCES+=$(patsubst %.proto, $(GEN_DIR)/%.grpc.pb.cc, $(GRPC_PROTOS))

GEN_HEADERS=$(patsubst %.proto, $(GEN_DIR)/%.pb.h, $(PROTOS))
GEN_HEADERS+=$(patsubst %.proto, $(GEN_DIR)/%.grpc.pb.h, $(GRPC_PROTOS))

ifeq ("$(BUILD_TESTS)", "True")
  BASE_SOURCES:=$(SOURCES)
  SOURCES=$(filter-out src/main.cpp,$(BASE_SOURCES)) $(shell find testcases -type f -name '*.cpp' -o -name '*.cc' -o -name '*.c')  
  CPPFLAGS+=-DTEST_BUILD -DCATCH_BUILD -DCATCH_CONFIG_PREFIX_ALL -DCATCH_CONFIG_COLOUR_ANSI
  LIBS+=-lCatch2Main -lCatch2
endif

ifeq ("$(BUILD_EXAMPLES)", "True")
  SOURCES+= $(shell find examples  -type f -name '*.cpp' -o -name '*.cc' -o -name '*.c')
  CPPFLAGS+=-DEXAMPLES_BUILD
endif

ifeq ("$(BENCHMARK)", "True")
  SOURCES+= $(shell find benchmark -type f -name '*.cpp' -o -name '*.cc' -o -name '*.c')
  CPPFLAGS+=-DBENCHMARK_BUILD
  LIBS+=-lbenchmark
endif

# ---------------------------------------------------------------------------- Include base makefile
# Check that we're in the correct directory
MKFILE_PATH:=$(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR:=$(patsubst %/,%,$(dir $(MKFILE_PATH)))
ifneq ("$(MKFILE_DIR)", "$(CURDIR)")
  $(error Should run from $(MKFILE_DIR) not $(CURDIR))
endif

# Add base makefile rules
NIGGLY_ROOT_DIR:=$(CURDIR)/modules/niggly
BASE_MAKE_FILE:=$(NIGGLY_ROOT_DIR)/toolchain-config/base.inc.makefile
include $(BASE_MAKE_FILE)

# -------------------------------------------------------------------------------------------- Rules
# Standard Rules, many described in 'base.inc.makefile'
.PHONY: clean info test deps test-scan module-deps coverage coverage_html $(TARGET)

all: $(TARGET_DIR)/$(TARGET)

test: | all
	$(TARGET_DIR)/$(TARGET)

run: | all
	$(TARGET_DIR)/$(TARGET)

$(TARGET): $(TARGET_DIR)/$(TARGET)

$(BUILD_DIR)/lib/libabsl.a: $(shell find $(INSTALL_PREFIX)/lib -type f -name 'libabsl_*.a')
	@echo "$(BANNER)libabsl.a$(BANEND)"
	rm -rf $(dir $@)tmp
	mkdir -p $(dir $@)tmp
	cd $(dir $@)tmp ; for X in $^ ; do mkdir $$(basename $$X) ; cd $$(basename $$X) ; $(AR) -x $$X ; cd .. ; done ; $(AR) -rcs $@ */*.o
	$(RANLIB) $@
	rm -rf $(dir $@)tmp
	@$(RECIPETAIL)



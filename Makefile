#
# make build=/tmp/build_captchagen path_fir=/home/kolodin/forge fir_path_build=/tmp/build_fir g=0 all
#

NAME=var-printer

.PHONY: all clean makedirs
.SUFFIXES:

ARCH = $(shell arch)

#
# Warning: All .d files will go to MAKEFILES_LIST
#
mkfile_path = $(abspath $(firstword $(MAKEFILE_LIST)))
mkfile_dir = $(abspath $(dir $(mkfile_path)))
makefile_parent_dir = $(abspath $(dir $(abspath $(mkfile_dir))))

#name_of_makefile_parent_dir_without_path := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

DEFAULT_BUILD_PATH = /mnt/ramdisk/build_${NAME}
DEFAULT_FIR_INCLUDE = /home/kolodin/forge
DEFAULT_FIR_BUILD = /mnt/ramdisk/build_fir

BLD = $(if $(build),$(build),${DEFAULT_BUILD_PATH})
fir_path_include = $(if $(fir_include),$(fir_include),${DEFAULT_FIR_INCLUDE})
fir_path_build = $(if $(fir_build),$(fir_build),${DEFAULT_FIR_BUILD})
DEP = ${BLD}/dep
OBJ = ${BLD}/objs
LIB = ${BLD}/lib
EXE = ${BLD}/bin

DIRS_TO_CREATE = \
${BLD} \
${DEP} \
${OBJ} \
${LIB} \
${EXE}

OBJS= \
	${OBJ}/main.o 

CINCLUDE = -I${makefile_parent_dir} -I${fir_path_include}

# add later: "-Wfloat-conversion"
CXXFLAGS = \
-Wuninitialized -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-sign-compare -Wno-type-limits -Wunused-function \
-ggdb -fno-strict-aliasing -fwrapv -fno-stack-protector \
-fno-strict-overflow \
-std=c++11

CFLAGS_X86_64 = -mpclmul -march=core2 -mfpmath=sse -mssse3 -m64
ifeq (${ARCH}, x86_64)
	CXXFLAGS += ${CFLAGS_X86_64}
endif

STRIP=strip
ifeq (${g}, 1)
    CXXFLAGS += -O0 -g -rdynamic
		STRIP="ls"
else
    CXXFLAGS += -O3 -g
endif

ifeq (${asan}, 1)
  CXXFLAGS += -fsanitize=address
endif

LDFLAGS =


DEP_FILES = $(OBJS:${OBJ}/%.o=${DEP}/%.d)
-include ${DEP_FILES}

#DEP_FILES = $(addprefix ${DEP}/, $(OBJECTS_FIR:${OBJ}/%.o=%.d))
#-include ${DEP_FILES}

#
# common targets
#

${DIRS_TO_CREATE}:
	@echo "create dir: "$@
	test -d $@ || mkdir $@

makedirs: ${DIRS_TO_CREATE}

clean:
	@echo "Cleaning "${BLD}
	rm -rf ${BLD}

${DEP}/%.d: src/%.cpp | makedirs
	@echo "===> DEP ("$@")"
	${CXX} ${CINCLUDE} ${CXXFLAGS} -MP -MMD -MT $(@:${DEP}/%.d=${OBJ}/%.o) -MF $@ ${<} -c -o $(@:${DEP}/%.d=${OBJ}/%.o)

${OBJ}/%.o: src/%.cpp | makedirs
	@echo "===> OBJ ("$@")"
	${CXX} ${CINCLUDE} ${CXXFLAGS} $< -c -o $@


${NAME}: ${OBJS}
	#${CXX} ${CINCLUDE} ${CXXFLAGS} ${LDFLAGS} ${OBJS} ${fir_path_build}/lib/fir.a -pthread -lpthread -lz -ljpeg -lpng -o ${EXE}/$@
	${CXX} ${CINCLUDE} ${CXXFLAGS} ${LDFLAGS} ${OBJS} -pthread -lpthread -lz -ljpeg -lpng -o ${EXE}/$@
	@echo "${STRIP} " ${EXE}/$@
	${STRIP} ${EXE}/$@

all: ${NAME}

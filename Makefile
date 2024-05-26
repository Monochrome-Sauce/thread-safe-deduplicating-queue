CXX := g++
CXXFLAGS := -std=c++17 -O2 -Werror -Wall -Wextra

BUILD_DIR := build
TARGET    := ${BUILD_DIR}/queue-test

SOURCE_FILES := main.cpp
SOURCE_OBJECTS := $(addprefix ${BUILD_DIR}/, $(addsuffix .o, ${SOURCE_FILES}))


_mkdir := mkdir --verbose --parents --
_rmdir := rm --verbose --one-file-system --recursive --
_display_recipe_header  = @echo -e '\n\e[95m>>> $@\e[0m: \e[90m$^\e[0m'


.PHONY: all build clean run
.SECONDARY: ${SOURCE_OBJECTS}

all: build

${TARGET}: ${SOURCE_OBJECTS} ;${_display_recipe_header}
	${CXX} ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

-include $(addsuffix .mk, ${SOURCE_OBJECTS})
$(info Makefiles: $(strip ${MAKEFILE_LIST}))
$(info Sources: $(strip ${SOURCE_FILES}))

${BUILD_DIR}/%.cpp.o: %.cpp ;${_display_recipe_header}
	${_mkdir} $(dir $@)
	${CXX} -c $< ${CXXFLAGS} -o $@ -MMD -MF $@.mk


build: ${TARGET}

clean: ;${_display_recipe_header}
	-${_rmdir} '${BUILD_DIR}'

run: build ;${_display_recipe_header}
	exec ${TARGET}

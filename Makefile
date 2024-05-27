CXX := g++
CXXFLAGS := -std=c++17 -O2 -Werror -Wall -Wextra

BUILD_DIR := build
TARGET    := ${BUILD_DIR}/queue-test

SOURCE_FILES := main.cpp
SOURCE_OBJECTS := $(addprefix ${BUILD_DIR}/, $(addsuffix .o, ${SOURCE_FILES}))


_mkdir := mkdir --verbose --parents --
_rmdir := rm --verbose --one-file-system --recursive --
_display_recipe_header  = @echo -e '\n\e[95m>>> $@\e[m: \e[90m$^\e[m'


.PHONY: help build clean run
.SECONDARY: ${SOURCE_OBJECTS}

help: ;${_display_recipe_header}
	@printf ' \e[33mmake\e[m:\n    Short for `\e[33mmake help\e[m`.\n'
	@printf ' \e[33mmake build\e[m:\n    Build the binary and create Make files to track changes in the header files.\n'
	@printf ' \e[33mmake clean\e[m:\n    Remove auxilary files and build files.\n'
	@printf ' \e[33mmake help\e[m:\n    Show this help message.\n'
	@printf ' \e[33mmake run\e[m:\n    Execute the binary resulting from `make build`.\n'
	@printf 'TLDR: `\e[33mmake clean build run\e[m`\n'

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

run: ;${_display_recipe_header}
	exec '${TARGET}'

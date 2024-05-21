CXX := g++
CXXFLAGS := -std=c++17 -Og -g
CPPFLAGS := -Werror -Wall -Wextra
CPPFLAGS += $(addprefix -Wno-error=,unused-parameter unused-variable)
CPPFLAGS += $(addprefix -fsanitize=,undefined unreachable null bounds)

BUILD_DIR := build
TARGET    := ${BUILD_DIR}/queue-test

SOURCE_FILES := main.cpp
SOURCE_OBJECTS := $(addprefix ${BUILD_DIR}/, $(addsuffix .o, ${SOURCE_FILES}))


_mkdir := mkdir --verbose --parents --
_rmdir := rm --verbose --one-file-system --recursive --
_display_recipe_header  = @echo -e '\n\e[95m>>> $@\e[0m: \e[90m$^\e[0m'


.PHONY: all build clean compile_commands run
.SECONDARY: ${SOURCE_OBJECTS}

all: compile_commands build

${TARGET}: ${SOURCE_OBJECTS} ;${_display_recipe_header}
	${CXX} ${CXXFLAGS} ${CPPFLAGS} -o $@ $^ ${LDFLAGS}

-include $(addsuffix .mk, ${SOURCE_OBJECTS})
$(info Makefiles: $(strip ${MAKEFILE_LIST}))
$(info Sources: $(strip ${SOURCE_FILES}))

${BUILD_DIR}/%.cpp.o: %.cpp ;${_display_recipe_header}
	${_mkdir} $(dir $@)
	${CXX} -c $< ${CXXFLAGS} ${CPPFLAGS} -o $@ -MMD -MF $@.mk


define JqFilter :=
'[inputs|{                                                     \
	directory: "$(abspath .))",                            \
	file     : match(" -c [^ ]+").string[4:],              \
	command  : sub(" -c [^ ]+ ";" -c ") | sub(" -o .*";"") \
}]'
endef

${BUILD_DIR}/compile_commands.json: Makefile $(patsubst %/,%,$(dir ${SOURCE_FILES})) ;${_display_recipe_header}
	${_mkdir} $(dir $@)
	${MAKE} ${TARGET} --always-make --dry-run  \
	  | grep '^${CXX} '           \
	  | jq -nR --tab ${JqFilter} \
	  > $@


build: ${TARGET}

clean: ;${_display_recipe_header}
	-${_rmdir} '${BUILD_DIR}'

compile_commands: ${BUILD_DIR}/compile_commands.json

run: build ;${_display_recipe_header}
	./${TARGET}

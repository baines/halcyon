BACKEND := linux

LIB_OUT := halcyon.a
LIB_SRC := $(wildcard lib/*.c) $(wildcard lib/$(BACKEND)/*.c)
LIB_HDR := $(wildcard lib/*.h) $(wildcard lib/$(BACKEND)/*.h)
LIB_OBJ := $(patsubst lib/%.c,build/%.o,$(LIB_SRC))

PROJECTS := $(filter-out common, $(notdir $(wildcard src/*)))
$(foreach p,$(PROJECTS),$(eval PRJ_SRC_$(p) := $(wildcard src/$(p)/*.c) $(wildcard src/common/*.c)))
$(foreach p,$(PROJECTS),$(eval PRJ_HDR_$(p) := $(wildcard src/$(p)/*.h) $(wildcard src/common/*.h)))
$(foreach p,$(PROJECTS),$(eval PRJ_OBJ_$(p) := $(patsubst src/%.c,build/%.o,$(PRJ_SRC_$(p)))))
$(foreach p,$(PROJECTS),$(eval PRJ_RES_$(p) := $(wildcard src/$(p)/res/*)))
$(foreach p,$(PROJECTS),$(eval PRJ_DAT_$(p) := $(patsubst src/$(p)/res/%,build/$(p)/%.res.o,$(PRJ_RES_$(p)))))
$(foreach p,$(PROJECTS),$(eval PRJ_VAR_$(p) := $(addprefix @,$(wildcard src/$(p)/*.mk))))
$(foreach p,$(PROJECTS),$(eval PRJ_VAR_$(p) := $(if $(PRJ_VAR_$(p)),$(PRJ_VAR_$(p)),@default_vars.mk)))

CFLAGS ?= -g -Wall -ffast-math -msse -msse2
CC = gcc

include lib/$(BACKEND)/vars.mk

all: $(LIB_OUT) $(PROJECTS)

$(LIB_OUT): $(LIB_OBJ)
	ar rcs $@ $^

.SECONDARY:
.SECONDEXPANSION:

$(PROJECTS): $$(PRJ_OBJ_$$@) $$(PRJ_DAT_$$@) $$(PRJ_HDR_$$@) $(LIB_OUT)
	$(CC) -I. $(CFLAGS) $(PRJ_OBJ_$@) $(PRJ_DAT_$@) $(PRJ_VAR_$@) -o $@ $(LDFLAGS)

$(LIB_OBJ): build/%.o: lib/%.c $(LIB_HDR) halcyon.h halcyonix.h | build/$(BACKEND)/
	$(CC) -I. $(CFLAGS) -c $< -o $@

build/%.res.o: src/$$(notdir $$(@D))/res/$$(notdir %) | $$(@D)/
	(cd $(<D) && ld -r -b binary -z noexecstack $(<F) -o ../../../$@)

build/%.o: src/%.c $$(PRJ_HDR_$$(notdir $$(@D))) | $$(@D)/
	$(CC) -I. $(CFLAGS) -Isrc/common -c $< -o $@

build/%/:
	mkdir -p $@

clean:
	$(RM) -r build $(LIB_OUT) $(PROJECTS)

.PHONY: clean all

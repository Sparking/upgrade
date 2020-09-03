CROSS_COMPILE :=
CC            := $(CROSS_COMPILE)gcc
AR            := $(CROSS_COMPILE)ar

CFLAGS := -g -O0
CPPFLAGS := -Wall -Werror -std=gnu99 -MMD -Iinclude -DTEST
LDFLAGS  :=
LIBS     := -ljson-c

src := common.c rbtree.c iniparser.c package.c
# upgrade.c
src := $(addprefix src/,$(src))
deps:= $(patsubst %.c,%.d,$(src))
objs:= $(patsubst %.c,%.o,$(src))

outoput := upgrade

.PHONY: all
all: $(outoput)

$(outoput): $(objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

-include $(deps)

$(objs): %.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	$(RM) $(outoput)
	$(RM) $(deps)
	$(RM) $(objs)

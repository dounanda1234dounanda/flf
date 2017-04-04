PROG := flf

LDLIBS := -lncursesw -lm
SRCS := flf.c fixed_file_io.c fixed_file_util.c

OTHERS := hex.pl Makefile

TARFILE := flf.tar
ZIPFILE := flf_$(shell hostname)_$(shell date +%Y%m%d).zip
ZIPPASS := 1qaz2wsx

CC := clang



CRELEASEFLAGS := -s -O2

CPROFFLAGS := -g -O2 -pg
LDPROFFLAGS := -pg

CFLAGS := -Wall -std=gnu89 -g -MMD -MP -O0

INDENT := indent -kr -l80 -nut

OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.d)
HEDS := $(SRCS:%.c=%.h)

$(PROG): $(OBJS)

all: clean $(PROG)

%.o: %.c
	$(INDENT) $<
	$(CC) $(CFLAGS) -c $<

.PHONY	: release
release	: CFLAGS+=$(CRELEASEFLAGS)
release	: clean
release	: all
release	: tarball
release	: zip

.PHONY	: profile
profile	: CFLAGS+=$(CPROFFLAGS)
profile	: LDFLAGS+=$(LDPROFFLAGS)
profile	: clean
profile	: all

.PHONY : tarball
tarball:
	tar cvf $(TARFILE) $(SRCS) $(HEDS) $(PROG) $(OTHERS)
	gzip -f $(TARFILE)

.PHONY : clean
clean:
	rm -f $(OBJS) $(DEPS) *~ $(PROG) $(TARFILE).gz $(ZIPFILE)

.PHONY : zip
zip:
	( cd ../; zip -P$(ZIPPASS) flf/$(ZIPFILE) $(addprefix flf/,$(SRCS) $(HEDS) $(OTHERS)); )

-include $(DEPS)

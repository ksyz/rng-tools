##
## Toplevel Makefile.am for rng-tools
##

SUBDIRS		= contrib

sbin_PROGRAMS	 = rngd
bin_PROGRAMS	 = rngtest
man_MANS	 = rngd.8 rngtest.1
noinst_LIBRARIES = librngd.a

rngd_SOURCES	= rngd.h rngd.c	rngd_entsource.h rngd_entsource.c	\
		  rngd_linux.h rngd_linux.c util.c 			\
		  rngd_rdrand.c rdrand_asm.S
rngd_LDADD	= librngd.a

rngtest_SOURCES	= exits.h stats.h stats.c rngtest.c
rngtest_LDADD	= librngd.a

librngd_a_SOURCES = fips.h fips.c


EXTRA_DIST	= autogen.sh

Android.mk: Makefile.am
	/home/xyzz/Sources/androgenizer/androgenizer \
		-:PROJECT rng-tools \
		\
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:SHARED librngd \
		-:TAGS optional \
		-:CFLAGS $(DEFS) \
		-:SOURCES $(librngd_a_SOURCES) \
		\
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:TAGS optional \
		-:EXECUTABLE rngd \
		-:SOURCES $(rngd_SOURCES) \
		-:CFLAGS $(DEFS) \
		-:LDFLAGS -lrngd \
		\
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:TAGS optional \
		-:EXECUTABLE rngtest \
		-:SOURCES $(rngtest_SOURCES) \
		-:CFLAGS $(DEFS) \
		-:LDFLAGS -lrngd \
		\
> $@

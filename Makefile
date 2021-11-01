prefix=/usr/local
exec_prefix=$(prefix)
libdir=$(exec_prefix)/lib

# build with `make OPUSENC=0` to disable rewrite support using libopusenc
OPUSENC?=1

CFLAGS=-pthread -D_FORTIFY_SOURCE=2 -fPIC
DEBUG=-g3
OPTIMIZE=-O3
CPPFLAGS=
DEFS=
INSTALL=/usr/bin/install -c
LDFLAGS=-pthread -Wl,--warn-common
LIBS=
SHELL=/bin/sh

ASTMODDIR=$(libdir)/asterisk/modules
MODULES=codec_opus_open_source format_ogg_opus_open_source format_vp8 res_format_attr_opus

.SUFFIXES: .c .so

.PHONY: all clean install uninstall $(MODULES)

all: $(MODULES)

clean:
	rm -f */*.so

install: $(MODULES)
	$(INSTALL) -D -t $(DESTDIR)$(ASTMODDIR) */*.so

uninstall:
	cd $(ASTMODDIR) && rm -f $(addsuffix .so,$(MODULES))

codec_opus_open_source: LIBS+=-lopus
codec_opus_open_source: DEFS+=-DAST_MODULE=\"codec_opus_open_source\" \
	-DAST_MODULE_SELF_SYM=__internal_codec_opus_open_source_self
codec_opus_open_source: codecs/codec_opus_open_source.so

format_ogg_opus_open_source: CPATH+=-I/usr/include/opus
format_ogg_opus_open_source: LIBS+=-lopus -lopusfile
format_ogg_opus_open_source: DEFS+=-DAST_MODULE=\"format_ogg_opus_open_source\" \
	-DAST_MODULE_SELF_SYM=__internal_format_ogg_opus_open_source_self
ifeq ($(OPUSENC),1)
format_ogg_opus_open_source: LIBS+=-lopusenc
format_ogg_opus_open_source: DEFS+=-DHAVE_OPUSENC
endif
format_ogg_opus_open_source: formats/format_ogg_opus_open_source.so

format_vp8: DEFS+=-DAST_MODULE=\"format_vp8\" \
	-DAST_MODULE_SELF_SYM=__internal_format_vp8_self
format_vp8: formats/format_vp8.so

res_format_attr_opus: DEFS+=-DAST_MODULE=\"res_format_attr_opus\" \
	-DAST_MODULE_SELF_SYM=__internal_res_format_attr_opus_self
res_format_attr_opus: res/res_format_attr_opus.so

.c.so:
	$(CC) -o $@ $(CPATH) $(DEFS) $(CPPFLAGS) $(CFLAGS) $(DEBUG) $(OPTIMIZE) $(LIBS) -shared $(LDFLAGS) $<

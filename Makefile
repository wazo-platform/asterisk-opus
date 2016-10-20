ASTMODDIR=/usr/lib64/asterisk/modules
MODULES=codec_opus_open_source format_ogg_opus_open_source format_vp8 res_format_attr_opus
LIBS=

.PHONY: all clean install uninstall $(MODULES)

all: $(MODULES)

%.so: %.c
	gcc -o $@ -pthread -g3 -O3 -D_FORTIFY_SOURCE=2 -fPIC -DAST_MODULE=\"$(*F)\" -shared -Wl,--warn-common $< $(LIBS)

clean:
	rm -f */*.so

install: $(MODULES)
	cp */*.so $(ASTMODDIR)

uninstall:
	cd $(ASTMODDIR) && rm $(addsuffix .so,$(MODULES))

codec_opus_open_source: LIBS=-lopus
codec_opus_open_source: codecs/codec_opus_open_source.so

format_ogg_opus_open_source: LIBS=-lopus -lopusfile
format_ogg_opus_open_source: codecs/format_ogg_opus_open_source.so

format_vp8: formats/format_vp8.so

res_format_attr_opus: res/res_format_attr_opus.so

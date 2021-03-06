#  Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
# 
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
# 
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
#  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
#  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
#  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

DESTDIR  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
OFLAGS   = -Os
CFLAGS   = -Wall
LIBS     = -lX11 -lXtst -lXi

BINARY   = bindbutton
SOURCE   = bindbutton.cc

all: $(BINARY)

.PHONY: all clean

clean:
	$(RM) $(BINARY)

$(BINARY): $(SOURCE)
	$(CXX) $(OFLAGS) $(CFLAGS) $(LIBS) $< -o $@

install: all
	install -Ds $(BINARY) $(DESTDIR)$(BINDIR)/$(BINARY)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(BINARY)

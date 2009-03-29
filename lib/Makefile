OBJS =		metadata.o metafield.o sha1.o http.o torrent.o peer.o \
		connection.o hasher.o file.o overseer.o uploader.o tracer.o
CXXFLAGS =	-I../include/tortilla -g -Wall -Werror
LDFLAGS +=	-lssl -lcurl
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread


libtortilla.a:	$(OBJS)
		$(AR) cr libtortilla.a $(OBJS)

clean:
		rm -f libtortilla.a $(OBJS)

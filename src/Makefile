TARGET =	tortilla
OBJS =		main.o metadata.o metafield.o sha1.o http.o torrent.o peer.o \
		connection.o hasher.o file.o overseer.o uploader.o
CXXFLAGS +=	-I../include -g
LDFLAGS +=	-lssl -lcurl
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread

torilla:	$(OBJS)
		$(CXX) -o tortilla $(OBJS) $(LDFLAGS)

clean:
		rm -f tortilla $(OBJS)

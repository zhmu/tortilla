TARGET =	tortilla
OBJS =		main.o metadata.o metafield.o sha1.o http.o torrent.o peer.o \
		connection.o hasher.o file.o overseer.o
CXXFLAGS = 	-I../include
CXXFLAGS +=	-g
LDFLAGS +=	-lssl -lcurl

torilla:	$(OBJS)
		$(CXX) -o tortilla $(OBJS) $(LDFLAGS)

clean:
		rm -f tortilla $(OBJS)

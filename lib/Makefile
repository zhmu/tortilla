OBJS =		metadata.o metafield.o sha1.o http.o torrent.o peer.o \
		connection.o hasher.o file.o overseer.o sender.o tracer.o \
		pendingpeer.o senderrequest.o filemanager.o
CXXFLAGS =	-I../include/tortilla -g -Wall
LDFLAGS +=	-lssl -lcurl
#CXXFLAGS +=	-DDEBUG_LOCKING
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread


libtortilla.a:	$(OBJS)
		$(AR) cr libtortilla.a $(OBJS)

clean:
		rm -f libtortilla.a $(OBJS)

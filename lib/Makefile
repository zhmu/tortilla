OBJS =		metadata.o metafield.o sha1.o httprequest.o torrent.o peer.o \
		connection.o hasher.o file.o overseer.o sender.o tracer.o \
		pendingpeer.o senderrequest.o filemanager.o receiver.o \
		info.o trackertalker.o
CXXFLAGS =	-I../include/tortilla -g -Wall
LDFLAGS +=	-lssl
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
# Below is needed for Linux
CXXFLAGS +=	-D_FILE_OFFSET_BITS=64
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread


libtortilla.a:	$(OBJS)
		$(AR) cr libtortilla.a $(OBJS)

clean:
		rm -f libtortilla.a $(OBJS)

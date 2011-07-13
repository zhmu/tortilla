OBJS =		main.o interface.o overview.o info.o
CXXFLAGS =	-I../include -I../include/client -g -Wall -Werror
LIBS =		../lib/libtortilla.a
LDFLAGS +=	-lncurses
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
# Below is needed for Linux
CXXFLAGS +=	-D_FILE_OFFSET_BITS=64
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread -lboost_thread

all:		tortilla yoctorrent

tortilla:	$(OBJS) $(LIBS)
		$(CXX) -o tortilla $(OBJS) $(LIBS) $(LDFLAGS)

yoctorrent:	yoctorrent.cc $(LIBS)
		$(CXX) -o yoctorrent yoctorrent.cc $(CXXFLAGS) $(LIBS) $(LDFLAGS)

clean:
		rm -f tortilla yoctorrent $(OBJS)

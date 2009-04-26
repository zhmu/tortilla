OBJS =		main.o interface.o overview.o info.o
CXXFLAGS =	-I../include -I../include/client -g -Wall -Werror
LIBS =		../lib/libtortilla.a
LDFLAGS +=	-lssl -lcurl -lncurses
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread

all:		tortilla yoctorrent

tortilla:	$(OBJS) $(LIBS)
		$(CXX) -o tortilla $(OBJS) $(LIBS) $(LDFLAGS)

yoctorrent:	yoctorrent.cc $(LIBS)
		$(CXX) -o yoctorrent yoctorrent.cc $(CXXFLAGS) $(LIBS) $(LDFLAGS)

clean:
		rm -f tortilla yoctorrent $(OBJS)

OBJS =		main.o interface.o overview.o info.o
CXXFLAGS =	-I../include -I../include/client -g -Wall -Werror
LIBS =		../lib/libtortilla.a
LDFLAGS +=	-lssl -lcurl -lncurses
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread

tortilla:	$(OBJS)
		$(CXX) -o tortilla $(OBJS) $(LIBS) $(LDFLAGS)

clean:
		rm -f tortilla $(OBJS)

R=		../..
CXXFLAGS =	-I$R/include -g -Wall -Werror
LIBS =		$R/lib/libtortilla.a
# Below are flags that are needed for FreeBSD
CXXFLAGS += 	-I/usr/local/include
# Below is needed for Linux
CXXFLAGS +=	-D_FILE_OFFSET_BITS=64
LDFLAGS +=	-L/usr/local/lib
LDFLAGS +=	-pthread -lboost_thread

$(TARGET):	$(OBJS) $(LIBS)
		$(CXX) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

clean:
		rm -f $(TARGET) $(OBJS)

TARGET =	tortilla
OBJS =		main.o metadata.o metafield.o
CXXFLAGS = 	-I../include
CXXFLAGS +=	-g

torilla:	$(OBJS)
		$(CXX) -o tortilla $(OBJS)

clean:
		rm -f tortilla $(OBJS)

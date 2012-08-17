TARGETS   = gdbstub

SRCS      = gdbstub.cpp \
	    Socket.cpp \
	    Port.cpp \
	    RSP.cpp \
	    Processor.cpp

OBJS      = $(SRCS:.cpp=.o)

CXXFLAGS += -ggdb -g3
LDFLAGS  += -ggdb -g3

all: gdbstub

.cpp.o:
	$(CXX) -o $@ -c $^ $(CXXFLAGS) 

$(TARGETS):
	$(CXX) -o $@ $^ $(LDFLAGS)
	objcopy --only-keep-debug $@ $@.sym
	objcopy --strip-debug $@
	objcopy --add-gnu-debuglink=$@.sym $@

gdbstub:  \
	Socket.o \
	Port.o \
	RSP.o \
	Processor.o \
	gdbstub.o

clean:
	rm -f *.o gdbstub *.sym

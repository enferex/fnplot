CC=gcc
CXX=g++
CSRCS=main.c
CXXSRCS=build.cc
COBJS=$(CSRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cc=.oo)
OBJS=$(COBJS) $(CXXOBJS)
CFLAGS=-g3 -O0 -std=c99 $(EXTRA_CFLAGS) -Wall -pedantic
CXXFLAGS=-g3 -O0 -std=c++11 -pedantic -Wall
APP=fnplot

all: $(APP)

$(APP): $(OBJS) 
	$(CXX) $^ $(LIBS) $(CXXFLAGS) -o $@

%.o: %.c
	$(CC) -c $^ $(CFLAGS) -o $@

%.oo: %.cc
	$(CXX) -c $^ $(CXXFLAGS) -o $@

.PHONY: test
test: $(APP)
	./$(APP) cscope.out

clean:
	$(RM) $(APP) $(OBJS)

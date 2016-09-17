CXX=g++
CXXSRCS=main.cc cs.cc
COBJS=$(CSRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cc=.oo)
OBJS=$(COBJS) $(CXXOBJS)
CXXFLAGS=-g3 -O0 -std=c++11 -pedantic -Wall
APP=fnplot

all: $(APP)

$(APP): $(OBJS) 
	$(CXX) $^ $(LIBS) $(CXXFLAGS) -o $@

%.oo: %.cc
	$(CXX) -c $^ $(CXXFLAGS) -o $@

.PHONY: test
test: $(APP)
	./$(APP) -c cscope.out -f "foo" -o foo.dot

clean:
	$(RM) $(APP) $(OBJS)

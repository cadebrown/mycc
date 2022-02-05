# makefile - defines building procedures for mycc, a C compiler

# -- Config --


LLVM_CONFIG   ?= llvm-config
#LLVM_CONFIG   ?= llvm-config-13

#CC             = clang
#CXX            = clang

CXXFLAGS      += -Wno-init-list-lifetime -g -std=c++11 -DLEFTTORIGHT `$(LLVM_CONFIG) --cxxflags`
LDFLAGS       += -Wl,-znodelete

LIBS           = `$(LLVM_CONFIG) --libs core native --ldflags` -lpthread -ldl -lz -ltinfo -ll


# -- Files --

src_CC        := $(wildcard src/*.cc) src/gram.cc src/lex.cc
src_H         := $(wildcard include/*.h) include/gram.h

src_CC_O      := $(patsubst %.cc,%.o,$(src_CC))

out           := mycc


# -- Rules --

$(out): $(src_CC_O)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

src/gram.cc: src/gram.y
	bison --yacc -vd -Wno-conflicts-sr $<
	mv y.tab.c $@
	mv y.tab.h include/gram.h

src/lex.cc: src/lex.l src/gram.cc
	flex $<
	mv lex.yy.c $@


%.o: %.cc $(src_H)
	$(CXX) $(CXXFLAGS) -Iinclude $< -c -fPIC -fPIE -o $@

clean:
	rm -f $(wildcard $(src_CC_O) $(out) src/gram.cc src/lex.cc)


.PHONY: clean

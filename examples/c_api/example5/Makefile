
Tgt	:= myecho
Src := $(wildcard *.c)
Objs := $(Src:.c=.o)
 
INCDIR = -I/usr/include
LIBDIR = -L/usr/lib
LIBS = -ljack -lluajack -lpthread

COPT	+= -O2
COPT	+= -Wall -Wextra -Wpedantic
COPT    += -std=gnu99
COPT    += -fpic

override CFLAGS = $(COPT) $(INCDIR)

default: build

clean:
	@-rm -f *.so *.dll *.o *.err *.map *.S *~ *.log

build:	clean $(Tgt) 

$(Tgt):		$(Objs)
	@-$(CC) -shared -o $(Tgt).so $(Objs) $(LIBDIR) $(LIBS)
	@-rm -f $(Objs)
	@echo


GCC=gcc -g
CFLAGS=-c -Wall -Werror
SFLAG=-fpic
INCLUDE=lib/
BINDIR=build/
OBJDIR=build/
LIBDIR=build/
SRCDIR=lib/
APPDIR=app/
APPOBJ=app/
SAMPLEDIR=sampleapp/
SRCEXT=c
OBJEXT=o
SYSREPO_LFLAG=-lsysrepo
YANG_LFLAG=-lyang
DL_LFLAG=-ldl
SSH_LFLAG=-lssh
PS_MP_LFLAG=-lPS_MP
SOURCES_LIB=$(wildcard $(SRCDIR)*.c)
OBJECTS_LIB := $(patsubst $(SRCDIR)%,$(OBJDIR)%,$(SOURCES_LIB:.$(SRCEXT)=.$(OBJEXT)))
SOURCES_APP=$(wildcard $(APPDIR)*.c)
OBJECTS_APP := $(patsubst $(APPDIR)%,$(APPOBJ)%,$(SOURCES_APP:.$(SRCEXT)=.$(OBJEXT)))
MP_LIB_DIR=/home/user/PureSoftware/MP_3.0/lib/

# $@, The name of target
# $^, The name of all depencency 
# $<, The name of first depencency
# $?, The name of all depencencies newer than the target space seperated file
# $+, The name of all dependencies seperated by space with dupicated names included and in the same order as in the rules.

all: $(BINDIR) $(BINDIR)libPS_MP.a $(BINDIR)ruapp
$(BINDIR):
	mkdir -p $@

$(BINDIR)libPS_MP.a:$(OBJECTS_APP)  $(OBJECTS_LIB)
	@echo "\nCreating dynamic library"
	echo $^ 
#	$(GCC) $^  $(SSH_LFLAG) $(SYSREPO_LFLAG) $(YANG_LFLAG) -o $@
	ld -r $^ -o build/MP_LIB.o
	ar rcs $@ build/MP_LIB.o
	
	cp $(BINDIR)libPS_MP.a $(MP_LIB_DIR) 
	@echo "Done, created $@"    

$(OBJDIR)%.o: $(SRCDIR)%.c
	@echo "\nCompiling $<"
	$(GCC) -Ilib/ -Iapp/ $(CFLAGS) $(SFLAG) -I$(INCLUDE)  $(SSH_LFLAG) $(SYSREPO_LFLAG) $(YANG_LFLAG) -lpthread $<  -o $@
	@echo "Done"

$(APPOBJ)%.o: $(APPDIR)%.c
	@echo "\nCompiling $<"
	$(GCC) -Ilib/ -Iapp/ $(CFLAGS) $(SFLAG)  -I$(INCLUDE) $(SSH_LFLAG) $(SYSREPO_LFLAG) $(YANG_LFLAG)  -lpthread $< -o $@
	@echo "Done"

$(BINDIR)libdbsubscribe.so:$(OBJECTS_APP)
	@echo "compiling $^"
	$(GCC) $(APPDIR)subscribe_oran_apis.o -o $@

$(BINDIR)ruapp: $(SAMPLEDIR)main.c
	@echo "\nCreating ruapp $^ $@"
	$(GCC) -Ilib/ -Iapp/ $^  -L$(MP_LIB_DIR) $(PS_MP_LFLAG) $(SSH_LFLAG) $(SYSREPO_LFLAG) $(YANG_LFLAG) $(DL_LFLAG) -lpthread -o $@
	@echo "Done"

clean:
	rm -rf $(BINDIR)*.o $(BINDIR)*.a $(BINDIR)ruapp $(APPOBJ)*.o

objclean:
	rm -rf $(BINDIR)*.o $(APPOBJ)*.o

install:
	cp -rf $(BINDIR)ruapp /home/user/PureSoftware/MP_3.0/bin/
#	cp -rf $(BINDIR)libdbsubscribe.so /usr/local/lib
#	$(SYSREPO_LFLAG) $(YANG_LFLAG) $(SSH_LFLAG) $(PS_MP_LFLAG) $(DL_LFLAG) 

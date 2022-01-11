GCC=gcc -g
CFLAGS=-c -Wall -Werror -lz 
SFLAG=-fpic
INCLUDE=lib/ -Isw_management/
BINDIR=build/
OBJDIR=build/
LIBDIR=build/
SRCDIR=lib/
APPDIR=app/
APPOBJ=app/
SWDIR=sw_management/
SWOBJ=sw_management/
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
SW_LIB=$(wildcard $(SWDIR)*.c)
SW_APP:=$(patsubst $(SWDIR)%,$(SWOBJ)%,$(SW_LIB:.$(SRCEXT)=.$(OBJEXT)))

MP_LIB_DIR=/home/user/PureSoftware/MP_3.0/lib/
MP_BIN_DIR=/home/user/PureSoftware/MP_3.0/bin/

# $@, The name of target
# $^, The name of all depencency 
# $<, The name of first depencency
# $?, The name of all depencencies newer than the target space seperated file
# $+, The name of all dependencies seperated by space with dupicated names included and in the same order as in the rules.

all: $(BINDIR) $(BINDIR)libPS_MP.a $(BINDIR)ruapp
$(BINDIR):
	mkdir -p $@

$(BINDIR)libPS_MP.a:$(OBJECTS_APP)  $(OBJECTS_LIB $(SW_APP)
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
$(SWOBJ)%.o: $(SWDIR)%.c
	@echo "\nCompiling $<"
	$(GCC) -Isw_management $(CFLAGS) $(SFLAG) -I$(INCLUDE) -lz  $<  -o $@
	@echo "Done"

$(BINDIR)ruapp: $(SAMPLEDIR)main.c
	@echo "\nCreating ruapp $^ $@"
	$(GCC) -Ilib/ -Iapp/  -Isw_management $^  -L$(MP_LIB_DIR) $(PS_MP_LFLAG) $(SSH_LFLAG) $(SYSREPO_LFLAG) $(YANG_LFLAG) $(DL_LFLAG) -lz -lpthread -o $@
	@echo "Done"

clean:
	rm -rf $(BINDIR)*.o $(BINDIR)*.a $(BINDIR)ruapp $(APPOBJ)*.o $(SWOBJ)*.o

objclean:
	rm -rf $(BINDIR)*.o $(APPOBJ)*.o

install:
	cp -rf $(BINDIR)ruapp $(MP_BIN_DIR)
#	cp -rf $(BINDIR)libdbsubscribe.so /usr/local/lib
#	$(SYSREPO_LFLAG) $(YANG_LFLAG) $(SSH_LFLAG) $(PS_MP_LFLAG) $(DL_LFLAG) 

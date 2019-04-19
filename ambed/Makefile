#copyright (C) by Thomas A. Early, N7TAE

# If you are going to change this path, you will
# need to update the systemd service script
BINDIR=/usr/local/bin

GCC=g++
CFLAGS=-c -W -Wall -MMD -MD -std=c++11
# uncomment the next line for gdb debugging support
#CFLAGS=-c -ggdb3 -W -Wall -MMD -MD -std=c++11
LDFLAGS=-pthread
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
DEPENDS=$(SOUCESS:.cpp=.d)
EXECUTABLE=ambed

all : $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(GCC) $(LDFLAGS) $(OBJECTS) -lftd2xx -Wl,-rpath,/usr/local/lib -o $@

.cpp.o :
	$(GCC) $(CFLAGS) $< -o $@

clean :
	$(RM) $(EXECUTABLE) *.o *.d

-include $(DEPENDS)

# The install and uninstall targets need to be run by root
install :
#	Uncomment the following line for a compact, stripped binary
#	strip -p $(EXECUTABLE)
	/bin/cp -f $(EXECUTABLE) $(BINDIR)
	/bin/cp -f ../systemd/ambed.service /lib/systemd/system/
	if [ ! -f /etc/default/ambed ]; then cp ../config/ambed /etc/default; fi
	systemctl enable ambed
	systemctl daemon-reload
	systemctl start ambed

uninstall :
	systemctl stop ambed
	systemctl disable ambed
	/bin/rm -f /lib/systemd/system/ambed.service
	systemctl daemon-reload
	/bin/rm -f $(BINDIR)/ambed

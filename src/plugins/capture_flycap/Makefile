# makefile
#
TARGET		:= capture_flycap

CFLAGS		:= $(CFLAGS) -I ./dev/include/flycapture -I ./dev/include/flycapture/C
LIBS		:= $(LIBS) -L ./dev/lib64 -lflycapture -lflycapture-c

all:	$(TARGET).so

%.so: %.o
	$(CXX) -shared $^ -o $@ $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c -MD $< $(CFLAGS) $(addprefix -I, $(SRCDIRLIST))

include $(wildcard *.d)

.PHONY: clean

clean:
	@rm -f *.d *.o *.so

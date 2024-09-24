TARGET  = switcher
CROSS   = arm-linux-
CXXFLAGS  = -I/opt/staging_dir/target/usr/include/SDL2 -D_REENTRANT
LDFLAGS = -L/opt/staging_dir/target/rootfs/usr/miyoo/lib
LDFLAGS += -lSDL2 -lSDL2_image -lSDL2_ttf

export PATH=/opt/a30/bin:$(shell echo $$PATH)

all:
	$(CROSS)g++ *.cpp *.c -o $(TARGET) $(CXXFLAGS) $(LDFLAGS) -static-libstdc++ -O3

clean:
	rm -rf $(TARGET)

# 编插件: make     (产物 build/libeu4cjk.dylib)
CC = xcrun clang
CFLAGS = -arch x86_64 -O2 -Wall -mmacosx-version-min=11.0
SRC = src/main.c src/patch.c src/hook.c src/font.c src/shot.c src/gl.c src/spy.c src/stubs.S
FW = -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation

build/libeu4cjk.dylib: $(SRC) src/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -dynamiclib -o $@ $(SRC) $(FW)

clean:
	rm -f build/libeu4cjk.dylib

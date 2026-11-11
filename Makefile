# 编插件: make     (产物 build/libeu4cjk.dylib)
CC = xcrun clang
CFLAGS = -arch x86_64 -O2 -Wall -mmacosx-version-min=11.0
SRC = src/main.c src/patch.c src/hook.c src/font.c src/shot.c src/gl.c src/spy.c src/fs.c src/input.c src/stubs.S
FW = -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation

build/libeu4cjk.dylib: $(SRC) src/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -dynamiclib -o $@ $(SRC) $(FW)

all: build/libeu4cjk.dylib build/eu4cjk-tool

clean:
	rm -f build/libeu4cjk.dylib build/eu4cjk-tool

# 安装工具 (苹果芯片和英特尔都能跑)
build/eu4cjk-tool: installer/eu4cjk-tool.c
	@mkdir -p build
	$(CC) -arch arm64 -arch x86_64 -O2 -Wall -mmacosx-version-min=11.0 -o $@ $<

tool: build/eu4cjk-tool

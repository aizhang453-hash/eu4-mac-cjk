// 显卡那一层 (OpenGL): 游戏把图片交给显卡时我们看一眼。
// 中文模组的地图字体 zh-hans-map.dds 是 6400x7663 的 DXT5 压缩图, 高度不是 4 的倍数,
// 苹果的 OpenGL 不收 (Windows 的 DirectX 收), 地图上的字就成了透明的。
// DXT 压缩图是 4x4 一块存的, 文件里其实已经存够 7664 行, 把高度补成 4 的倍数就行。
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

void cjk_log(const char *fmt, ...);

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT  0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

static void my_glCompressedTexImage2D(GLenum target, GLint level, GLenum fmt, GLsizei w, GLsizei h,
                                      GLint border, GLsizei size, const void *data) {
    int dxt = fmt >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && fmt <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    GLsizei w2 = w, h2 = h;
    if (dxt && level == 0) {
        if (w % 4) w2 = (w + 3) & ~3;
        if (h % 4) h2 = (h + 3) & ~3;
    }
    while (glGetError() != GL_NO_ERROR) {}
    glCompressedTexImage2D(target, level, fmt, w2, h2, border, size, data);
    GLenum err = glGetError();
    if (w >= 4096 || h >= 4096 || err != GL_NO_ERROR || w2 != w || h2 != h)
        cjk_log("显卡收压缩图 %dx%d 格式 %x 层 %d 大小 %d -> 交的是 %dx%d, 错误 %x", w, h, fmt, level, size, w2, h2, err);
}

__attribute__((used)) static struct { const void *replacement, *replacee; } interpose_gl
    __attribute__((section("__DATA,__interpose"))) = {
        (const void *)my_glCompressedTexImage2D, (const void *)glCompressedTexImage2D };

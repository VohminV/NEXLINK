// libwin_gdiplus.so - stub shim for gdiplus.dll (GDI+ graphics).

#include "common/logging.hpp"

#include <cstdint>

using HRESULT = int32_t;
using DWORD32 = uint32_t;
using DWORD = DWORD32;
using UINT = uint32_t;
using INT = int32_t;
using GpStatus = int32_t;
using GpGraphics = void*;
using GpBitmap = void*;
using GpBrush = void*;
using GpPen = void*;
using GpPath = void*;
using GpRegion = void*;
using GpFont = void*;
using GpFontFamily = void*;
using GpStringFormat = void*;
using GpImage = void*;
using GpPen = void*;
using GpSolidFill = void*;
using GpLineGradient = void*;
using GpTexture = void*;
using GpCustomLineCap = void*;
using GpImageAttributes = void*;
using GpMetafile = void*;
using GpHatch = void*;
using GpPathIterator = void*;
using GpRegionIterator = void*;
using GpBrush = void*;


using DWORD32 = uint32_t;
using ULONG_PTR = uintptr_t;
using GpWrapMode = int32_t;
using GpPoint = void*;
using WCHAR = wchar_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI GpStatus GdiplusStartup(ULONG_PTR* token, const void* input, void* output) {
    (void)token; (void)input; (void)output;
    return 0;
}
extern "C" WINAPI void GdiplusShutdown(ULONG_PTR token) {
    (void)token;
}
extern "C" WINAPI GpStatus GdipCreateGraphicsFromHDC(void* hdc, GpGraphics** graphics) {
    (void)hdc; (void)graphics;
    return 0;
}
extern "C" WINAPI GpStatus GdipDeleteGraphics(GpGraphics* graphics) {
    (void)graphics;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateBitmapFromScan0(INT width, INT height, INT stride, UINT format, void* scan0, GpBitmap** bitmap) {
    (void)width; (void)height; (void)stride; (void)format; (void)scan0; (void)bitmap;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateBitmapFromFile(const WCHAR* filename, GpBitmap** bitmap) {
    (void)filename; (void)bitmap;
    return 0;
}
extern "C" WINAPI GpStatus GdipDisposeImage(GpImage* image) {
    (void)image;
    return 0;
}
extern "C" WINAPI GpStatus GdipGetImageWidth(GpImage* image, UINT* width) {
    (void)image; (void)width;
    return 0;
}
extern "C" WINAPI GpStatus GdipGetImageHeight(GpImage* image, UINT* height) {
    (void)image; (void)height;
    return 0;
}
extern "C" WINAPI GpStatus GdipDrawImage(GpGraphics* graphics, GpImage* image, INT x, INT y) {
    (void)graphics; (void)image; (void)x; (void)y;
    return 0;
}
extern "C" WINAPI GpStatus GdipDrawImageRectI(GpGraphics* graphics, GpImage* image, INT x, INT y, INT width, INT height) {
    (void)graphics; (void)image; (void)x; (void)y; (void)width; (void)height;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateSolidFill(UINT argb, GpSolidFill** brush) {
    (void)argb; (void)brush;
    return 0;
}
extern "C" WINAPI GpStatus GdipDeleteBrush(GpBrush* brush) {
    (void)brush;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreatePen1(UINT color, INT width, UINT unit, GpPen** pen) {
    (void)color; (void)width; (void)unit; (void)pen;
    return 0;
}
extern "C" WINAPI GpStatus GdipDeletePen(GpPen* pen) {
    (void)pen;
    return 0;
}
extern "C" WINAPI GpStatus GdipFillRectangleI(GpGraphics* graphics, GpBrush* brush, INT x, INT y, INT width, INT height) {
    (void)graphics; (void)brush; (void)x; (void)y; (void)width; (void)height;
    return 0;
}
extern "C" WINAPI GpStatus GdipDrawRectangleI(GpGraphics* graphics, GpPen* pen, INT x, INT y, INT width, INT height) {
    (void)graphics; (void)pen; (void)x; (void)y; (void)width; (void)height;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateLineGradient(const GpPoint* p1, const GpPoint* p2, UINT color1, UINT color2, GpLineGradient** brush) {
    (void)p1; (void)p2; (void)color1; (void)color2; (void)brush;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateTexture(GpImage* image, GpWrapMode wrapMode, GpTexture** texture) {
    (void)image; (void)wrapMode; (void)texture;
    return 0;
}
extern "C" WINAPI GpStatus GdipCreateHatch(UINT hatchstyle, UINT foreColor, UINT backColor, GpHatch** brush) {
    (void)hatchstyle; (void)foreColor; (void)backColor; (void)brush;
    return 0;
}

using DWORD32 = uint32_t;
using ULONG_PTR = uintptr_t;
using GpWrapMode = int32_t;
using GpPoint = void*;
using WCHAR = wchar_t;

} // namespace win2lin

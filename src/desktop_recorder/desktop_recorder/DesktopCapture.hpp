#pragma once

#include <boost/scope_exit.hpp>

class DesktopCapture {
public:
	DesktopCapture()
	{
	}

	void start(int left, int top, int width, int height)
	{
		left_ = left; 
		top_ = top;
		width_ = width;
		height_ = height;
		bitmap_size_ = width * height * 4;
		bitmap_ = malloc(bitmap_size_);
		memset(bitmap_, 0, bitmap_size_);
	}

	void* getBitmap()
	{
		do_capture();
		return bitmap_;
	}

private:
	int left_, top_, width_, height_, bitmap_size_;
	void* bitmap_;

	void do_capture()
	{
		BITMAPINFO bmpInfo;
		HDC hMemDC = NULL;
		HDC hScrDC = NULL;
		HBITMAP hBit = NULL;
		HBITMAP hOldBitmap = NULL;

		BOOST_SCOPE_EXIT(&hMemDC, &hScrDC, &hBit, &hOldBitmap)
		{
			if (hMemDC != NULL) {
				if (hOldBitmap != NULL) SelectObject(hMemDC, hOldBitmap);
				DeleteDC(hMemDC);
			}

			if (hBit != NULL) DeleteObject(hBit);
			if (hScrDC != NULL) DeleteDC(hScrDC);
		}
		BOOST_SCOPE_EXIT_END;

		hScrDC = GetDC(0);
		if (hScrDC == NULL) {
			DebugOutput::trace("GetDC() Error %d", GetLastError());
			return;
		}

		hMemDC = CreateCompatibleDC(hScrDC);
		if (hMemDC == NULL) {
			DebugOutput::trace("CreateCompatibleDC() Error %d", GetLastError());
			return;
		}

		hBit = CreateCompatibleBitmap(hScrDC, width_, height_);
		if (hBit == NULL) {
			DebugOutput::trace("CreateCompatibleBitmap() Error %d", GetLastError());
			return;
		}

		hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBit);

		if (BitBlt(hMemDC, 0, 0, width_, height_, hScrDC, left_, top_, SRCCOPY) == FALSE) {
			DebugOutput::trace("BitBlt() Error %d", GetLastError());
			return;
		}

		// 마우스 위치 얻기
		POINT curPoint;
		GetCursorPos(&curPoint);

		curPoint.x -= GetSystemMetrics(SM_XVIRTUALSCREEN);
		curPoint.y -= GetSystemMetrics(SM_YVIRTUALSCREEN);

		// hcurosr 얻어서 그리기
		{
			CURSORINFO cursorInfo;
			cursorInfo.cbSize = sizeof(CURSORINFO);

			if (GetCursorInfo(&cursorInfo)) {
				ICONINFO iconInfo;
				if (GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
					curPoint.x = curPoint.x - iconInfo.xHotspot;
					curPoint.y = curPoint.y - iconInfo.yHotspot;
					if (iconInfo.hbmMask != NULL) DeleteObject(iconInfo.hbmMask);
					if (iconInfo.hbmColor != NULL) DeleteObject(iconInfo.hbmColor);
				}
				DrawIconEx(hMemDC, curPoint.x - left_, curPoint.y - top_, cursorInfo.hCursor, 0, 0, 0, NULL, DI_NORMAL);
			}
		}

		bmpInfo.bmiHeader.biBitCount = 0;
		bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpInfo.bmiHeader.biWidth = width_;
		bmpInfo.bmiHeader.biHeight = -height_;
		bmpInfo.bmiHeader.biPlanes = 1;
		bmpInfo.bmiHeader.biBitCount = PIXEL_SIZE * 8;
		bmpInfo.bmiHeader.biCompression = BI_RGB;
		bmpInfo.bmiHeader.biSizeImage = 0;
		bmpInfo.bmiHeader.biXPelsPerMeter = 0;
		bmpInfo.bmiHeader.biYPelsPerMeter = 0;
		bmpInfo.bmiHeader.biClrUsed = 0;
		bmpInfo.bmiHeader.biClrImportant = 0;

		GetDIBits(hMemDC, hBit, 0, height_, (PBYTE) bitmap_, &bmpInfo, DIB_RGB_COLORS);
	}
};

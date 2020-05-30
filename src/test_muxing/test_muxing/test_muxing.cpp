#define __STDC_CONSTANT_MACROS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ryulib/VideoCreater.hpp>

#define STREAM_DURATION 10.0

int main(int argc, char **argv)
{
    VideoCreater* videoCreater = new VideoCreater("test.mp4", 1024, 768);

    // 인코딩을 확인하기 위해서 위 아래 색상이 다른 bitmap 샘플 생성
    void* bitmap = malloc(1024 * 768 * 4);
    memset(bitmap, 255, 1024 * 768 * 2);
    char* temp_bitmap = (char*) bitmap;
    temp_bitmap = temp_bitmap + 1024 * 768 * 2;
    memset(temp_bitmap, 0, 1024 * 768 * 2);

    void* audio = malloc(1024 * 1024);
    memset(audio, 0, 1024 * 1024);

    while (true) {
        if (videoCreater->isVideoTurn()) {
            if (videoCreater->writeBitmap(bitmap) == false) break;
        } else {
            if (videoCreater->writeAudioPacket(audio, 1024) == false) break;
        }

        if (videoCreater->isEOF(STREAM_DURATION)) break;
    }

    delete videoCreater;

    return 0;
}

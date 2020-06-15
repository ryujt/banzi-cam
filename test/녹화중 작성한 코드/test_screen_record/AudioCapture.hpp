#pragma once

#include <ryulib/AudioIO.hpp>
#include <ryulib/ThreadQueue.hpp>

const int CHANNELS = 1;
const int SAMPLERATE = 44100;
const int SAMPLESIZE = 4;
const int FRAMES = 4410;

class AudioCapture {
public:
	AudioCapture()
		: audioInput_(CHANNELS, SAMPLERATE, SAMPLESIZE, FRAMES)
	{
		Audio::init();

		audioInput_.setOnData([&](const void* obj, const void* data, int size) {
			audio_size_ = size;
			void* temp = malloc(size);
			memcpy(temp, data, size);
			queue_.push(temp);
		});
	}

	void start()
	{
		audioInput_.open();
	}

	void stop()
	{
		audioInput_.close();
	}

	void* getAudioData()
	{
		void* data = queue_.pop();
		if (data == NULL) return nullptr;
		return data;
	}

	int getAudioDataSize()
	{
		return audio_size_;
	}

private:
	int audio_size_ = 0;
	AudioInput audioInput_;
	ThreadQueue<void*> queue_;
};
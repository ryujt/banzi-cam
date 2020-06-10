#pragma once

#include <ryulib/base.hpp>
#include <ryulib/debug_tools.hpp>
#include <ryulib/AudioIO.hpp>
#include <ryulib/ThreadQueue.hpp>

const int CHANNELS = 1;
const int SAMPLE_RATE = 44100;
const int SAMPLE_SIZE = 4;
const int FRAMES_PER_BUFFER = 4410;

class AudioCapture {
public:
	AudioCapture()
		: audio_input_(CHANNELS, SAMPLE_RATE, SAMPLE_SIZE, FRAMES_PER_BUFFER)
	{
		Audio::init();

		audio_input_.setOnError([&](const void* obj, int error_code) {
			DebugOutput::trace("AudioCapture - error_code: %d", error_code);
		});

		audio_input_.setOnData([&](const void* obj, const void* buffer, int buffer_size) {
			audio_size_ = buffer_size;
			void* data = malloc(buffer_size);
			memcpy(data, buffer, buffer_size);
			queue_.push(data);
			//DebugOutput::trace("AudioCapture - queue_.size(): %d", queue_.size());
		});
	}

	void start()
	{
		audio_input_.open();
	}

	void stop()
	{
		audio_input_.close();
	}

	void* getAudioData()
	{
		void* data = queue_.pop();
		if (data == NULL) return nullptr;
		return data;
	}

	int getAduioDataSize() {
		return audio_size_;
	}

private:
	int audio_size_ = 0;
	AudioInput audio_input_;
	ThreadQueue<void*> queue_;
};

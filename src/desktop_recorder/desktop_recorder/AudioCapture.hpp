#pragma once

class AudioCapture {
public:
	AudioCapture()
	{
		audio_ = malloc(1024 * 1024);
		memset(audio_, 0, 1024 * 1024);

	}

	void start()
	{

	}

	void stop()
	{

	}

	void* getAudioData()
	{
		return audio_;
	}

	int getAduioDataSize() {
		return 1024;
	}

private:
	// TODO: 테스트를 위한 임시
	void* audio_;
};

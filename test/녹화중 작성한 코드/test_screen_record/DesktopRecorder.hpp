#pragma once

#include <string>
#include <ryulib/VideoCreater.hpp>
#include <ryulib/Scheduler.hpp>
#include "DesktopCapture.hpp"
#include "AudioCapture.hpp"

const int TASK_START = 1;
const int TASK_STOP = 2;

using namespace std;

class Task {
public:
	Task(const string filename, int width, int height)
		: filename_(filename), width_(width), height_(height)
	{
	}
	string filename_;
	int width_, height_;
};

class DesktopRecorder {
public:
	DesktopRecorder()
	{
		scheduler_.setOnTask([&](int task, const string text, const void* data, int size, int tag) {
			switch (task)
			{
			case TASK_START: 
				do_start((Task*)data);
				break;
			case TASK_STOP: 
				do_stop();
				break;
			}
		});

		scheduler_.setOnRepeat([&]() {
			if (videoCreater_ == nullptr) {
				scheduler_.sleep(1);
				return;
			}

			if (do_encode() == false) {
				scheduler_.sleep(1);
			}
		});
	}

	void start(const string filename, int width, int height)
	{
		scheduler_.add(TASK_START, new Task(filename, width, height));
	}

	void stop()
	{
		scheduler_.add(TASK_STOP);
	}

	void terminate()
	{
		scheduler_.terminateAndWait();
		do_stop();
	}

private:
	VideoCreater* videoCreater_ = nullptr;
	Scheduler scheduler_;
	DesktopCapture desktopCapture_;
	AudioCapture audioCapture_;

	void do_start(Task* task)
	{
		if (videoCreater_ != nullptr)
			throw "이미 녹화 중입니다.";

		videoCreater_ = new VideoCreater(task->filename_.c_str(), task->width_, task->height_, CHANNELS, SAMPLERATE);
		desktopCapture_.start(0, 0, task->width_, task->height_);
		audioCapture_.start();
		scheduler_.start();
	}

	void do_stop()
	{
		scheduler_.stop();
		desktopCapture_.stop();
		audioCapture_.stop();

		while (true)
		{
			if (do_encode() == false) break;
		}

		if (videoCreater_ != nullptr) {
			delete videoCreater_;
			videoCreater_ = nullptr;
		}
	}

	bool do_encode()
	{
		if (videoCreater_ == nullptr) return false;

		if (videoCreater_->isVideoTurn()) {
			void* bitmap = desktopCapture_.getBitmap();
			if (videoCreater_->writeBitmap(bitmap) == false)
				throw "비디오 인코딩 중에 에러가 발생하였습니다.";
		}
		else {
			void* audio = audioCapture_.getAudioData();
			if (audio == nullptr) return false;

			int audio_size = audioCapture_.getAudioDataSize();
			if (videoCreater_->writeAudioPacket(audio, audio_size) == false)
				throw "오디오 인코딩 중에 에러가 발생하였습나다.";

			free(audio);
		}

		return true;
	}
};
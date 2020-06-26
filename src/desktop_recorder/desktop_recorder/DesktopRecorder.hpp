#pragma once

#include <ryulib/Scheduler.hpp>
#include <ffmpeg/VideoCreater.hpp>
#include "DesktopCapture.hpp"
#include "AudioCapture.hpp"

#define TASK_START	1
#define TASK_STOP	2

class Task {
public:
	Task(const string filename, int left, int top, int width, int height)
		: filename_(filename), left_(left), top_(top), width_(width), height_(height)
	{
	}

	string filename_;
	int left_, top_, width_, height_;
};

class DesktopRecorder {
public:
	DesktopRecorder()
	{
		scheduler_.setOnRepeat([&]() {
			if (videoCreater_ == nullptr) {
				scheduler_.sleep(1);
				return;
			}

			if (do_encode() == false) scheduler_.sleep(1);
		});

		scheduler_.setOnTask([&](int task, const string text, const void* data, int, int size) {
			switch (task) {
				case TASK_START: do_start((Task*) data); break;
				case TASK_STOP:  do_stop(); break;
			}
		});
	}

	void terminate()
	{
		scheduler_.terminateAndWait();
		do_stop();
	}

	void start(const string filename, int left, int top, int width, int height)
	{
		scheduler_.add(TASK_START, new Task(filename, left, top, width, height));
	}

	void stop()
	{
		scheduler_.add(TASK_STOP);
	}

private:
	VideoCreater* videoCreater_ = nullptr;
	Scheduler scheduler_;
	DesktopCapture desktopCapture_;
	AudioCapture audioCapture_;

	void do_start(Task* task)
	{
		if (videoCreater_ != nullptr) {
			throw "이미 녹화 중입니다.";
		}

		videoCreater_ = new VideoCreater(task->filename_.c_str(), task->width_, task->height_, CHANNELS, SAMPLE_RATE);
		desktopCapture_.start(task->left_, task->top_, task->width_, task->height_);
		audioCapture_.start();
		scheduler_.start();
	}

	void do_stop()
	{
		scheduler_.stop();
		audioCapture_.stop();

		while (true) {
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
		} else {
			void* audio = audioCapture_.getAudioData();
			if (audio == nullptr) return false;

			int audio_size = audioCapture_.getAduioDataSize();
			if (videoCreater_->writeAudioPacket(audio, audio_size) == false)
				throw "오디오 인코딩 중에 에러가 발생하였습니다.";

			free(audio);
		}

		return true;
	}
};

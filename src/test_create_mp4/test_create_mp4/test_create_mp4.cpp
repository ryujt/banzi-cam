#include <stdio.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

using namespace std;

//string fileName = "D:/Work/create.mp4";
string fileName = "D:/Work/create.mkv";

AVCodec* codec_video;
AVCodecContext* ctx_video;

AVCodec* codec_audio;
AVCodecContext* ctx_audio;

const int pixel_size = 3;
const int width = 1024;
const int height = 768;
const int video_framerate = 25;
const int video_bitrate = 1024 * 1024;
const void* bitmap = malloc(width * height * pixel_size);

const int sample_size = 4;
const int channels = 2;
const int samplerate = 48000;
const int audio_bitrate = 32 * 1024;
const int samples = 1024;
const void* audio_buffer = malloc(channels * samples * sample_size);

AVStream* add_video_stream(AVFormatContext* oc, enum AVCodecID codec_id)
{
	AVStream* st = avformat_new_stream(oc, 0);
	if (!st) return NULL;

	codec_video = avcodec_find_encoder(codec_id);
	if (!codec_video) return NULL;

	ctx_video = avcodec_alloc_context3(codec_video);
	if (!ctx_video) return NULL;

	ctx_video->codec_type = AVMEDIA_TYPE_VIDEO;
	ctx_video->width = width;
	ctx_video->height = height;
	ctx_video->bit_rate = video_bitrate;
	ctx_video->time_base.num = 1;
	ctx_video->gop_size = 10;
	ctx_video->max_b_frames = 1;
	ctx_video->pix_fmt = AV_PIX_FMT_YUV420P;
	av_opt_set(ctx_video->priv_data, "preset", "slow", 0);

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		ctx_video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(ctx_video, codec_video, NULL) < 0) return NULL;

	return st;
}

static int select_channel_layout(const AVCodec* codec)
{
	const uint64_t* p;
	uint64_t best_ch_layout = 0;
	int best_nb_channels = 0;

	if (!codec->channel_layouts)
		return AV_CH_LAYOUT_STEREO;

	p = codec->channel_layouts;
	while (*p) {
		int nb_channels = av_get_channel_layout_nb_channels(*p);

		if (nb_channels > best_nb_channels) {
			best_ch_layout = *p;
			best_nb_channels = nb_channels;
		}
		p++;
	}
	return best_ch_layout;
}

AVStream* add_audio_stream(AVFormatContext* oc, enum AVCodecID codec_id)
{
	AVStream* st = avformat_new_stream(oc, 0);
	if (!st) return NULL;

	codec_audio = avcodec_find_encoder(codec_id);
	if (!codec_audio) return NULL;

	ctx_audio = avcodec_alloc_context3(codec_audio);
	if (!ctx_audio) return NULL;

	ctx_audio->codec_type = AVMEDIA_TYPE_AUDIO;
	//ctx_audio->sample_fmt = AV_SAMPLE_FMT_FLTP;
	ctx_audio->sample_fmt = AV_SAMPLE_FMT_FLT;
	ctx_audio->channel_layout = select_channel_layout(codec_audio);
	ctx_audio->channels = channels;
	ctx_audio->sample_rate = samplerate;
	ctx_audio->bit_rate = audio_bitrate;

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		ctx_audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(ctx_audio, codec_audio, NULL) < 0) return NULL;

	return st;
}

int main()
{
	AVOutputFormat* fmt;
	fmt = av_guess_format(NULL, fileName.c_str(), NULL);
	if (!fmt) return -1;

	AVFormatContext* oc;
	avformat_alloc_output_context2(&oc, fmt, NULL, fileName.c_str());
	if (!oc) return -2;

	fmt->video_codec = AV_CODEC_ID_H264;
	//fmt->video_codec = AV_CODEC_ID_VP8;
	AVStream* video_st = add_video_stream(oc, fmt->video_codec);
	if (!video_st) return -3;

	//fmt->audio_codec = AV_CODEC_ID_AAC;
	//fmt->audio_codec = AV_CODEC_ID_OPUS;
	fmt->audio_codec = AV_CODEC_ID_MP3;
	AVStream* audio_st = add_audio_stream(oc, fmt->audio_codec);
	if (!video_st) return -4;

	if (avio_open(&oc->pb, fileName.c_str(), AVIO_FLAG_WRITE) < 0) return -5;

	avformat_write_header(oc, NULL);

	return 0;
}

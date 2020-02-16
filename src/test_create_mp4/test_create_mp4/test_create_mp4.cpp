#include <stdio.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "bcrypt.lib")

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}

using namespace std;

int pts = 0;

AVOutputFormat* fmt;
AVFormatContext* ctx;

AVCodec* codec_video;
AVCodecContext* ctx_video;
AVStream* st_video;
AVFrame* video_frame;
AVPacket* video_packet;

uint8_t* video_outbuf;
int video_outbuf_size;

AVCodec* codec_audio;
AVCodecContext* ctx_audio;
AVStream* st_audio;
AVFrame* audio_frame;
uint8_t* audio_outbuf;
AVPacket* audio_packet;

const int pixel_size = 4;
const int width = 1024;
const int height = 768;
const int video_framerate = 25;
const int video_bitrate = 1024 * 1024;
void* bitmap = malloc(width * height * pixel_size);

const int sample_size = 4;
const int samplerate = 48000;
const int audio_bitrate = 32 * 1024;

AVStream* add_video_stream(AVFormatContext* oc, enum AVCodecID codec_id)
{
	codec_video = avcodec_find_encoder(codec_id);
	if (!codec_video) return NULL;

	ctx_video = avcodec_alloc_context3(codec_video);
	if (!ctx_video) return NULL;

	ctx_video->codec_type = AVMEDIA_TYPE_VIDEO;
	ctx_video->width = width;
	ctx_video->height = height;
	ctx_video->bit_rate = video_bitrate;

	ctx_video->time_base.num = 1;
	ctx_video->time_base.den = video_framerate;
	ctx_video->framerate.num = video_framerate;
	ctx_video->framerate.den = 1;

	ctx_video->gop_size = 10;
	ctx_video->max_b_frames = 1;
	ctx_video->pix_fmt = AV_PIX_FMT_YUV420P;
	av_opt_set(ctx_video->priv_data, "preset", "slow", 0);

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		ctx_video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(ctx_video, codec_video, NULL) < 0) return NULL;

	return avformat_new_stream(oc, codec_video);
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
	codec_audio = avcodec_find_encoder(codec_id);
	if (!codec_audio) return NULL;

	ctx_audio = avcodec_alloc_context3(codec_audio);
	if (!ctx_audio) return NULL;

	ctx_audio->codec_type = AVMEDIA_TYPE_AUDIO;
	ctx_audio->sample_fmt = AV_SAMPLE_FMT_FLTP;
	//ctx_audio->sample_fmt = AV_SAMPLE_FMT_FLT;
	ctx_audio->channel_layout = select_channel_layout(codec_audio);
	ctx_audio->channels = av_get_channel_layout_nb_channels(ctx_audio->channel_layout);
	ctx_audio->sample_rate = samplerate;
	ctx_audio->bit_rate = audio_bitrate;

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		ctx_audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	codec_audio->capabilities = codec_audio->capabilities - AV_CODEC_CAP_EXPERIMENTAL;
	if (avcodec_open2(ctx_audio, codec_audio, NULL) < 0) return NULL;

	return avformat_new_stream(oc, codec_audio);
}

void write_audio_frame(void* data) {
	int result = av_frame_make_writable(audio_frame);
	if (result < 0) {
		printf("Error - av_frame_make_writable \n");
		return;
	}

	audio_frame->pts = pts;

	result = avcodec_send_frame(ctx_audio, audio_frame);
	if (result < 0) {
		printf("Error - sending the frame to the encoder \n");
		return;
	}

	while (result >= 0) {
		result = avcodec_receive_packet(ctx_audio, audio_packet);
		if (result < 0) return;

		av_interleaved_write_frame(ctx, audio_packet);
		av_packet_unref(audio_packet);
	}
}

void write_video_frame(void* bitmap) {
	int result = av_frame_make_writable(video_frame);
	if (result < 0) {
		printf("Error - av_frame_make_writable \n");
		return;
	}

	/* prepare a dummy image */
	/* Y */
	for (int y = 0; y < ctx_video->height; y++) {
		for (int x = 0; x < ctx_video->width; x++) {
			video_frame->data[0][y * video_frame->linesize[0] + x] = x + y ;
		}
	}

	/* Cb and Cr */
	for (int y = 0; y < ctx_video->height/2; y++) {
		for (int x = 0; x < ctx_video->width/2; x++) {
			video_frame->data[1][y * video_frame->linesize[1] + x] = 128 + y;
			video_frame->data[2][y * video_frame->linesize[2] + x] = 64 + x;
		}
	}

	video_frame->pts = pts;

	//memcpy(
	//	video_frame->data[0],
	//	bitmap,
	//	ctx_video->width * ctx_video->height * pixel_size
	//);

	result = avcodec_send_frame(ctx_video, video_frame);
	if (result < 0) {
		printf("Error - sending the frame to the encoder \n");
		return;
	}

	while (result >= 0) {
		result = avcodec_receive_packet(ctx_video, video_packet);
		if (result < 0) return;

		av_interleaved_write_frame(ctx, video_packet);
		av_packet_unref(video_packet);
	}
}

int create_video_file(string filename)
{
	fmt = av_guess_format(NULL, filename.c_str(), NULL);
	if (!fmt) return -1;

	avformat_alloc_output_context2(&ctx, fmt, NULL, filename.c_str());
	if (!ctx) return -2;

	//fmt->audio_codec = AV_CODEC_ID_AAC;
	fmt->audio_codec = AV_CODEC_ID_OPUS;
	//fmt->audio_codec = AV_CODEC_ID_MP3;
	st_audio = add_audio_stream(ctx, fmt->audio_codec);
	if (st_audio == NULL) return -4;

	avcodec_parameters_from_context(st_audio->codecpar, ctx_audio);

	fmt->video_codec = AV_CODEC_ID_H264;
	//fmt->video_codec = AV_CODEC_ID_VP8;
	st_video = add_video_stream(ctx, fmt->video_codec);
	if (st_video == NULL) return -3;

	avcodec_parameters_from_context(st_video->codecpar, ctx_video);

	if (avio_open(&ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) return -5;

	int result = avformat_write_header(ctx, NULL);
	if (result < 0) {
		printf("error - avformat_write_header: %d\n", result);
		return -9;
	}

	return 0;
}

void close_video_file()
{
	av_write_trailer(ctx);
	avcodec_close(ctx_video);
	avcodec_close(ctx_audio);
	avio_close(ctx->pb);
	avcodec_free_context(&ctx_video);
	avcodec_free_context(&ctx_video);
	avformat_free_context(ctx);
}

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture) return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) return NULL;

	return picture;
}

int main()
{
	int result = create_video_file("D:/Work/create2.mkv");
	if (result < 0) return result;

	video_frame = alloc_picture(AV_PIX_FMT_YUV420P, width, height);
	//video_frame = av_frame_alloc();
	//video_frame->format = ctx_video->pix_fmt;
	//video_frame->width  = ctx_video->width;
	//video_frame->height = ctx_video->height;
	result = av_frame_get_buffer(video_frame, 32);
	if (result < 0) {
		printf("Could not allocate the video frame data\n");
		return -7;
	}

	video_packet = av_packet_alloc();

	audio_frame = av_frame_alloc();
	audio_frame->nb_samples     = ctx_audio->frame_size;
	audio_frame->format         = ctx_audio->sample_fmt;
	audio_frame->channel_layout = ctx_audio->channel_layout;
	result = av_frame_get_buffer(audio_frame, 0);
	if (result < 0) {
		printf("Could not allocate audio data buffers\n");
		return -6;
	}

	audio_packet = av_packet_alloc();

	void* buffer = malloc(1024*1024);
	for (int i=0; i<(4096); i++) {
		//write_audio_frame(buffer);
		write_video_frame(bitmap);
		pts++;
	}

	close_video_file();

	return 0;
}

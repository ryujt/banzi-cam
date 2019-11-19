#include <stdio.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

using namespace std;

AVOutputFormat* fmt;
AVFormatContext* oc;

AVCodec* codec_video;
AVCodecContext* ctx_video;
AVStream* video_st;
uint8_t* video_outbuf;
int video_outbuf_size;

AVCodec* codec_audio;
AVCodecContext* ctx_audio;
AVStream* audio_st;
AVFrame* audio_frame;
uint8_t* audio_outbuf;
int audio_outbuf_size;

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
	ctx_audio->channels = channels;
	ctx_audio->sample_rate = samplerate;
	ctx_audio->bit_rate = audio_bitrate;

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		ctx_audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(ctx_audio, codec_audio, NULL) < 0) return NULL;

	return avformat_new_stream(oc, codec_audio);
}

bool write_audio_frame(void* data) {
	int result = av_frame_make_writable(audio_frame);
	if (result < 0) {
		printf("Error - av_frame_make_writable \n");
		return false;
	}

	result = avcodec_send_frame(ctx_audio, audio_frame);
	if (result < 0) {
		printf("Error - sending the frame to the encoder \n");
		return false;
	}

	AVPacket* pkt;
	pkt = av_packet_alloc();
	av_init_packet(pkt);

	while (result >= 0) {
		result = avcodec_receive_packet(ctx_audio, pkt);
		if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
			return true;
		else if (result < 0) {
			printf("Error encoding audio frame\n");
			return false;
		}

		av_interleaved_write_frame(oc, pkt);
		av_packet_unref(pkt);
	}

	return true;
}

int create_video_file(string filename)
{
	fmt = av_guess_format(NULL, filename.c_str(), NULL);
	if (!fmt) return -1;

	avformat_alloc_output_context2(&oc, fmt, NULL, filename.c_str());
	if (!oc) return -2;

	fmt->audio_codec = AV_CODEC_ID_AAC;
	//fmt->audio_codec = AV_CODEC_ID_OPUS;
	//fmt->audio_codec = AV_CODEC_ID_MP3;
	audio_st = add_audio_stream(oc, fmt->audio_codec);
	if (audio_st == NULL) return -4;

	avcodec_parameters_from_context(audio_st->codecpar, ctx_audio);

	fmt->video_codec = AV_CODEC_ID_H264;
	//fmt->video_codec = AV_CODEC_ID_VP8;
	video_st = add_video_stream(oc, fmt->video_codec);
	if (video_st == NULL) return -3;

	avcodec_parameters_from_context(video_st->codecpar, ctx_video);

	if (avio_open(&oc->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) return -5;

	int result = avformat_write_header(oc, NULL);
	if (result < 0) {
		printf("error - avformat_write_header: %d\n", result);
		return -9;
	}

	return 0;
}

void close_video_file()
{
	av_write_trailer(oc);
	avcodec_close(ctx_video);
	avcodec_close(ctx_audio);
	avio_close(oc->pb);
}

int main()
{
	int result = create_video_file("D:/Work/create.mp4");
	if (result < 0) return result;

	printf("ctx_audio->frame_size: %d \n", ctx_audio->frame_size);

	audio_frame = av_frame_alloc();
	audio_frame->nb_samples     = ctx_audio->frame_size;
	audio_frame->format         = ctx_audio->sample_fmt;
	audio_frame->channel_layout = ctx_audio->channel_layout;
	result = av_frame_get_buffer(audio_frame, 0);
	if (result < 0) {
		printf("Could not allocate audio data buffers\n");
		return -6;
	}

	result = av_frame_make_writable(audio_frame);
	if (result < 0) {
		printf("error - av_frame_make_writable\n");
		return -7;
	}

	void* buffer = malloc(1024*1024);
	for (int i=0; i<(1024); i++) write_audio_frame(buffer);

	close_video_file();

	return 0;
}

#define __STDC_CONSTANT_MACROS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ryulib/VideoCreater.hpp>

int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

static AVFrame *get_audio_frame(OutputStream *ost)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
    AVRational time_base;
    time_base.num = 1;
    time_base.den = 1;
    if (av_compare_ts(ost->next_pts, ost->enc->time_base, STREAM_DURATION, time_base) >= 0)
        return NULL;

    for (j = 0; j <frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++)
            *q++ = v;
        ost->t     += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;

    av_init_packet(&pkt);
    c = ost->enc;

    frame = get_audio_frame(ost);

    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
            c->sample_rate, c->sample_rate, AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
        * internally;
        * make sure we do not overwrite it here
        */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
            exit(1);

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
            ost->frame->data, dst_nb_samples,
            (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            exit(1);
        }
        frame = ost->frame;

        AVRational time_base;
        time_base.num = 1;
        time_base.den = c->sample_rate;
        frame->pts = av_rescale_q(ost->samples_count, time_base, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        fprintf(stderr, "Error encoding audio frame\n");
        exit(1);
    }

    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing audio frame\n");
            exit(1);
        }
    }

    return (frame || got_packet) ? 0 : 1;
}

int main(int argc, char **argv)
{
    VideoCreater* videoCreater = new VideoCreater("test.mp4", 1024, 768);

    void* bitmap = malloc(1024 * 768 * 4);
    memset(bitmap, 250, 1024 * 768 * 4);

    AVRational time_base;
    time_base.num = 1;
    time_base.den = 1;
    while (true) {
        if (videoCreater->isVideoTurn()) {
            if (videoCreater->writeBitmap(bitmap) == false) break;
        } else {
            if (write_audio_frame(videoCreater->oc, &videoCreater->audio_st)) break;
        }

        if (av_compare_ts(videoCreater->video_st.next_pts, videoCreater->video_st.enc->time_base, STREAM_DURATION, time_base) >= 0) break;
        if (av_compare_ts(videoCreater->audio_st.next_pts, videoCreater->audio_st.enc->time_base, STREAM_DURATION, time_base) >= 0) break;
    }

    delete videoCreater;

    return 0;
}

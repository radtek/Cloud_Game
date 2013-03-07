#pragma once
#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string>
using namespace std;
extern "C"
{
#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}


class StreamServer
{
public:
	StreamServer();
	~StreamServer();
	void setRemoteAddress(string address,int port);
	bool startStreamServer();
	void cleanup();
	bool write_video_frame(AVFrame *frame);
	bool write_audio_frame(AVFrame *frame);
	
private:
	AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *audio_st,*video_st;
    AVCodec *audio_codec, *video_codec;
    double audio_pts, video_pts;
	string remoteAddr;
	int remotePort;
	HANDLE g_hMutex;
private://VIDEO FUNCTION
	bool addVideoStream();
	bool openVideoContext();
	

private://AUDIO FUNCTION
	bool addAudioStream();
	bool openAudioContext();
	
};
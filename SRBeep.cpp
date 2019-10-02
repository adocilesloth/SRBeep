/***********************************
A Docile Sloth adocilesloth@gmail.com
************************************/

#include <obs-module.h>
#include <obs-frontend-api/obs-frontend-api.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <mutex>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "SDL.h"
#include "SDL_thread.h"
};

std::mutex audioMutex;
std::thread st_stt_Thread, st_sto_Thread, rc_stt_Thread, rc_sto_Thread;

#define	MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
static  Uint8 * audio_chunk;
static  Uint32  audio_len;
static  Uint8* audio_pos;

OBS_DECLARE_MODULE()

#ifdef _WIN32
#include <windows.h>

void psleep(unsigned milliseconds)
{
	Sleep(milliseconds);
}
#else
#include <unistd.h>

void psleep(unsigned milliseconds)
{
	usleep(milliseconds * 1000); // takes microseconds
}
#endif

void obs_module_unload(void)
{
	if (st_stt_Thread.joinable())
	{
		st_stt_Thread.join();
	}
	if (st_sto_Thread.joinable())
	{
		st_sto_Thread.join();
	}
	if (rc_stt_Thread.joinable())
	{
		rc_stt_Thread.join();
	}
	if (rc_sto_Thread.joinable())
	{
		rc_sto_Thread.join();
	}
	return;
}

const char* obs_module_author(void)
{
	return "A Docile Sloth";
}

const char* obs_module_name(void)
{
	return "Stream/Recording Start/Stop Beeps";
}

const char* obs_module_description(void)
{
	return "Adds audio sound when streaming/recording starts/stops.";
}

void fill_audio(void* udata, Uint8* stream, int len)
{
	/*****************************************************************
	From simplest_ffmpeg_audio_player by leixiaohua1020
	Download at https://sourceforge.net/projects/simplestffmpegplayer/
	*****************************************************************/
	//SDL 2.0
	SDL_memset(stream, 0, len);
	if (audio_len == 0)		/*  Only  play  if  we  have  data  left  */
		return;
	len = ((unsigned int)len > audio_len ? audio_len : len);	/*  Mix  as  much  data  as  possible  */

	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}

void play_clip(const char* filepath)
{
	//fix problems with audio_len being assigned a value
	static  Uint32  fixer;
	audio_len = fixer;
	//carry on
	/*****************************************************************
	Adapted from simplest_ffmpeg_audio_player by leixiaohua1020
	Download at https://sourceforge.net/projects/simplestffmpegplayer/
	*****************************************************************/
	AVFormatContext* stream_start = NULL;
	AVCodec* cdc = nullptr;
	int audioStreamIndex = -1;

	if (avformat_open_input(&stream_start, filepath, NULL, NULL) != 0)
	{
		blog(LOG_WARNING, "SRBeep: play_clip: Failed to open file");
		blog(LOG_WARNING, filepath);
		return;
	}

	if (avformat_find_stream_info(stream_start, NULL) < 0)
	{
		avformat_close_input(&stream_start);
		blog(LOG_WARNING, "SRBeep: play_clip: Failed to find stream_start_sound.mp3's stream info");
		return;
	}

	audioStreamIndex = av_find_best_stream(stream_start, AVMEDIA_TYPE_AUDIO, -1, -1, &cdc, 0);
	if (audioStreamIndex < 0)
	{
		avformat_close_input(&stream_start);
		blog(LOG_WARNING, "SRBeep: play_clip: Failed to find audio stream in stream_start_sound.mp3");
		return;
	}
	//get audio stream
	audioStreamIndex = -1;
	for (unsigned int i = 0; i < stream_start->nb_streams; i++)
	{
		if (stream_start->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStreamIndex = i;
			break;
		}
	}
	//get codec
	AVCodecContext* cdx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(cdx, stream_start->streams[audioStreamIndex]->codecpar);
	AVCodec* codec = avcodec_find_decoder(cdx->codec_id);
	if (!codec)
	{
		blog(LOG_WARNING, "SRBeep: play_clip: Codec not supported");
		return;
	}
	//openm codec
	avcodec_open2(cdx, codec, NULL);

	//audio packet
	AVPacket* packet;
	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Audio Parameters
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = cdx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
	uint8_t* out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	AVFrame* frame = av_frame_alloc();

	audioMutex.lock();

	//init SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		audioMutex.unlock();

		blog(LOG_WARNING, "SRBEEP: play_clip: SDL init failed");
		return;
	}

	SDL_AudioSpec wanted_spec;
	wanted_spec.freq = cdx->sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = cdx->channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = out_nb_samples;
	wanted_spec.callback = fill_audio;
	wanted_spec.userdata = cdx;

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{
		audioMutex.unlock();

		blog(LOG_WARNING, "SRBEEP: play_clip: SDL_OpenAudio failed");
		return;
	}

	//FIX:Some Codec's Context Information is missing
	int64_t in_channel_layout;
	in_channel_layout = av_get_default_channel_layout(cdx->channels);
	//Swr
	struct SwrContext* au_convert_ctx;
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout, cdx->sample_fmt, cdx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	int ret;
	int index = 0;

	while (av_read_frame(stream_start, packet) >= 0)
	{
		if (packet->stream_index == audioStreamIndex)
		{
			ret = avcodec_send_packet(cdx, packet);
			if (ret == AVERROR(EAGAIN))
				ret = 0;
			if (ret == 0)
			{
				ret = avcodec_receive_frame(cdx, frame);
			}
			if (ret < 0)
			{
				audioMutex.unlock();

				blog(LOG_WARNING, "SRBEEP: play_clip: Decoding audio frame error");
				return;
			}
			else {
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t**)frame->data, frame->nb_samples);
				index++;
			}

			while (audio_len > 0)//Wait until finish
				SDL_Delay(1);

			//Set audio buffer (PCM data)
			audio_chunk = (Uint8*)out_buffer;
			//Audio buffer length
			audio_len = out_buffer_size;
			audio_pos = audio_chunk;

			//Play

			SDL_PauseAudio(0);
		}
	}
	av_packet_free(&packet);
	swr_free(&au_convert_ctx);
	//Close SDL
	SDL_CloseAudio();
	SDL_Quit();
	//clean up
	av_free(out_buffer);
	avcodec_close(cdx);
	av_frame_free(&frame);
	avformat_close_input(&stream_start);
	avcodec_free_context(&cdx);
	audioMutex.unlock();
	return;
}

std::string clean_path(std::string audio_path)
{
	std::string cleaned_path;
	//If relative path then the first 2 chars should be ".."
	if (audio_path.find("..") != std::string::npos)
	{
		size_t pos = audio_path.find("..");
		cleaned_path = audio_path.substr(pos);
	}
	//If absolute path, Windows will start with a capital, Linux/Mac will start with "/"
	else
	{
#ifdef _WIN32
		while (islower(audio_path[0]) && audio_path.length() > 0)
		{
			audio_path = audio_path.substr(1);
		}
#else
		while (audio_path.substr(0, 1) != "/" && audio_path.length() > 0)
		{
			audio_path = audio_path.substr(1);
		}
#endif
		cleaned_path = audio_path;
	}
	return cleaned_path;
}

void start_stream_sound(void)
{
	const char* obs_data_path = obs_get_module_data_path(obs_current_module());
	std::stringstream audio_path;
	std::string true_path;

	audio_path << obs_data_path;
	audio_path << "/stream_start_sound.mp3";
	true_path = clean_path(audio_path.str());
	play_clip(true_path.c_str());
	audio_path.str("");

	return;
}

void stop_stream_sound(void)
{
	const char* obs_data_path = obs_get_module_data_path(obs_current_module());
	std::stringstream audio_path;
	std::string true_path;

	audio_path << obs_data_path;
	audio_path << "/stream_stop_sound.mp3";
	true_path = clean_path(audio_path.str());
	play_clip(true_path.c_str());
	audio_path.str("");

	return;
}

void start_record_sound(void)
{
	const char* obs_data_path = obs_get_module_data_path(obs_current_module());
	std::stringstream audio_path;
	std::string true_path;

	audio_path << obs_data_path;
	audio_path << "/record_start_sound.mp3";
	true_path = clean_path(audio_path.str());
	play_clip(true_path.c_str());
	audio_path.str("");

	return;
}

void stop_record_sound(void)
{
	const char* obs_data_path = obs_get_module_data_path(obs_current_module());
	std::stringstream audio_path;
	std::string true_path;

	audio_path << obs_data_path;
	audio_path << "/record_stop_sound.mp3";
	true_path = clean_path(audio_path.str());
	play_clip(true_path.c_str());
	audio_path.str("");

	return;
}

void obsstudio_srbeep_frontend_event_callback(enum obs_frontend_event event, void* private_data)
{
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED)
	{
		if (st_stt_Thread.joinable())
		{
			st_stt_Thread.join();
		}
		st_stt_Thread = std::thread(start_stream_sound);
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED)
	{
		if (rc_stt_Thread.joinable())
		{
			rc_stt_Thread.join();
		}
		rc_stt_Thread = std::thread(start_record_sound);
	}
	else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED)
	{
		if (st_sto_Thread.joinable())
		{
			st_sto_Thread.join();
		}
		st_sto_Thread = std::thread(stop_stream_sound);
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED)
	{
		if (rc_sto_Thread.joinable())
		{
			rc_sto_Thread.join();
		}
		rc_sto_Thread = std::thread(stop_record_sound);
	}
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(obsstudio_srbeep_frontend_event_callback, 0);
	return true;
}

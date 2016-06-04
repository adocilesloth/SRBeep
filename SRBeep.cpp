/************************************
A Docile Sloth adocilesloth@gmail.com
************************************/

#include <obs-module.h>
#include <thread>
#include <atomic>
#include <sstream>

extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswresample/swresample.h"
	#include "SDL.h"
	#include "SDL_thread.h"
};

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
static  Uint8  *audio_chunk; 
static  Uint32  audio_len; 
static  Uint8  *audio_pos; 

void main_loop(void);
bool is_streaming(void);
bool is_recording(void);

void play_clip(const char*);
void fill_audio(void*, Uint8*, int);

//thread stuff
std::thread SRBeepThread;
std::thread st_stt_Thread, st_sto_Thread, rc_stt_Thread, rc_sto_Thread;
std::atomic<bool> closed(false);

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

bool obs_module_load(void)
{
	av_register_all();
	//start thread
	SRBeepThread = std::thread(main_loop);
	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
	return;
#else
	closed = true;
	if(SRBeepThread.joinable())
	{
		SRBeepThread.join();
	}
	return;
#endif
}

const char *obs_module_author(void)
{
	return "A Docile Sloth";
}

const char *obs_module_name(void)
{
	return "Stream/Recording Start/Stop Beeps";
}

const char *obs_module_description(void)
{
	return "Adds audio sound when streaming/recording starts/stops.";
}

void main_loop(void)
{
	bool stream_outputting = false;
	bool record_outputting = false;
	const char* obs_data_path = obs_get_module_data_path(obs_current_module());
	std::stringstream audio_path;
	while(!closed)
	{
		psleep(100);

		//stream or recording starts
		if(!stream_outputting && is_streaming())
		{
			if(st_stt_Thread.joinable())
			{
				st_stt_Thread.join();
			}
			audio_path << obs_data_path;
			audio_path << "/stream_start_sound.mp3";
			st_stt_Thread = std::thread(play_clip, audio_path.str().c_str());
			audio_path.str("");
			stream_outputting = true;
		}
		if(!record_outputting && is_recording())
		{
			if(rc_stt_Thread.joinable())
			{
				rc_stt_Thread.join();
			}
			audio_path << obs_data_path;
			audio_path << "/record_start_sound.mp3";
			rc_stt_Thread = std::thread(play_clip, audio_path.str().c_str());
			audio_path.str("");
			record_outputting = true;
		}

		//stream or recording stops
		if(stream_outputting && !is_streaming())
		{
			if(st_sto_Thread.joinable())
			{
				st_sto_Thread.join();
			}
			audio_path << obs_data_path;
			audio_path << "/stream_stop_sound.mp3";
			st_sto_Thread = std::thread(play_clip, audio_path.str().c_str());
			audio_path.str("");
			stream_outputting = false;
		}
		if(record_outputting && !is_recording())
		{
			if(rc_sto_Thread.joinable())
			{
				rc_sto_Thread.join();
			}
			audio_path << obs_data_path;
			audio_path << "/record_stop_sound.mp3";
			rc_sto_Thread = std::thread(play_clip, audio_path.str().c_str());
			audio_path.str("");
			record_outputting = false;
		}
	}
	if(st_stt_Thread.joinable())
	{
		st_stt_Thread.join();
	}
	if(st_sto_Thread.joinable())
	{
		st_sto_Thread.join();
	}
	if(rc_stt_Thread.joinable())
	{
		rc_stt_Thread.join();
	}
	if(rc_sto_Thread.joinable())
	{
		rc_sto_Thread.join();
	}
}

bool is_streaming(void)
{
	if(obs_output_active(obs_get_output_by_name("simple_stream")))
	{
		return true;
	}
	else if(obs_output_active(obs_get_output_by_name("adv_stream")))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool is_recording(void)
{
	if(obs_output_active(obs_get_output_by_name("simple_file_output")))
	{
		return true;
	}
	else if(obs_output_active(obs_get_output_by_name("adv_file_output")))
	{
		return true;
	}
	else if(obs_output_active(obs_get_output_by_name("adv_ffmpeg_output")))
	{
		return true;
	}
	else
	{
		return false;
	}
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
	AVFormatContext *stream_start = NULL;
	AVCodec* cdc = nullptr;
	int audioStreamIndex = -1;
	
	if(avformat_open_input(&stream_start, filepath, NULL, NULL) != 0)
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
    if(audioStreamIndex < 0)
    {
        avformat_close_input(&stream_start);
        blog(LOG_WARNING, "SRBeep: play_clip: Failed to find audio stream in stream_start_sound.mp3");
		return;
    }
	//get audio stream
	audioStreamIndex = -1;
	for(unsigned int i = 0; i < stream_start->nb_streams; i++)
	{
		if(stream_start->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStreamIndex = i;
			break;
		}
	}
	//get codec
	AVCodecContext *cdx = stream_start->streams[audioStreamIndex]->codec;
	AVCodec *codec = avcodec_find_decoder(cdx->codec_id);
	if(!codec)
	{
		blog(LOG_WARNING, "SRBeep: play_clip: Codec not supported");
		return;
	}
	//openm codec
	avcodec_open2(cdx, codec, NULL);

	//audio packet
	AVPacket *packet;
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);
	
	//Audio Parameters
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples=cdx->frame_size;
	AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
	int out_sample_rate=44100;
	int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
	//Buffer Size
	int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);
	uint8_t	*out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
	AVFrame *frame = av_frame_alloc();
	
	//init SDL
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{  
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
	
	if(SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{
		blog(LOG_WARNING, "SRBEEP: play_clip: SDL_OpenAudio failed");
		return;
	}

	//FIX:Some Codec's Context Information is missing
	int64_t in_channel_layout;
	in_channel_layout = av_get_default_channel_layout(cdx->channels);
	//Swr
	struct SwrContext *au_convert_ctx;
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout, cdx->sample_fmt , cdx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	int ret, got_picture;
	int index = 0;
	
	while(av_read_frame(stream_start, packet) >= 0)
	{
		if(packet->stream_index == audioStreamIndex)
		{
			ret = avcodec_decode_audio4(cdx, frame, &got_picture, packet);
			if(ret < 0)
			{
				blog(LOG_WARNING, "SRBEEP: play_clip: Decoding audio frame error");
                return;
            }
			if(got_picture > 0)
			{
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)frame->data , frame->nb_samples);
				index++;
			}

			while(audio_len > 0)//Wait until finish
				SDL_Delay(1);
			
			//Set audio buffer (PCM data)
			audio_chunk = (Uint8 *) out_buffer; 
			//Audio buffer length
			audio_len = out_buffer_size;
			audio_pos = audio_chunk;

			//Play
			
			SDL_PauseAudio(0);
		}
		av_free_packet(packet);
	}
	
	swr_free(&au_convert_ctx);
	//Close SDL
	SDL_CloseAudio();
	SDL_Quit();
	//clean up
	av_free(out_buffer);
	avcodec_close(cdx);
	av_frame_free(&frame);
	avformat_close_input(&stream_start);

	return;
}

void  fill_audio(void *udata, Uint8 *stream, int len)
{ 
	/*****************************************************************
	From simplest_ffmpeg_audio_player by leixiaohua1020
	Download at https://sourceforge.net/projects/simplestffmpegplayer/
	*****************************************************************/
	//SDL 2.0
	SDL_memset(stream, 0, len);
	if(audio_len == 0)		/*  Only  play  if  we  have  data  left  */ 
			return; 
	len = ( (unsigned int)len > audio_len?audio_len:len);	/*  Mix  as  much  data  as  possible  */ 

	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len; 
	audio_len -= len; 
} 

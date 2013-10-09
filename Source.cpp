

#ifndef includes
extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/mem.h>
	#include <libavutil/time.h>
}

#include <SDL.h>
#endif

#undef main

#define FF_REFRESH_EVENT (SDL_USEREVENT)

#include "Video.cpp"
#include "Audio.cpp"
#include "Syncer.cpp"

class Multimedia
{	
	private:
		char * filename;

		AVFormatContext *formatContext;
		Video			*V;
		Audio			*A;
		Syncer			*Sync;

		static Uint32 PushRefreshEvent(Uint32 interval, void *userdata)
		{	
			SDL_Event e;
			e.type = FF_REFRESH_EVENT;
			SDL_PushEvent(&e);
			return 0;
		}
		bool			quitEvent;
	
		int videoStream;
		int audioStream;
		int getStreamID(AVMediaType type)
		{
			unsigned int i;
			for (i = 0; i < formatContext->nb_streams; i++)
				if (formatContext->streams[i]->codec->codec_type == type)
					return i;

			return -1;
		}

		static int DemuxThread(void *arg)
		{
			Multimedia *m = (Multimedia*)arg;
			m->Demux();
			return 0;
		}	

		int Demux()
		{
			AVPacket packet;
			while (av_read_frame(formatContext, &packet) >= 0 && !quitEvent)
			{
				if (packet.stream_index == videoStream)
					V->PutPacket(&packet);
				else if (packet.stream_index == audioStream)
					A->PutPacket(&packet);
			}

			return 0;
		}

		void StartEventLoop()
		{
			SDL_Event event;

			while (quitEvent != 1)
			{
				if (SDL_WaitEvent(&event)) {
					if (event.type == SDL_QUIT) {
						Quit();
					}
					if (event.type == FF_REFRESH_EVENT)
					{
						Uint32 delay = (int)(Sync->computeFrameDelay() * 1000);
						SDL_AddTimer(delay, PushRefreshEvent, NULL);
						V->RenderPicture();
					}
				}
			}
		}

	public:

		Multimedia()
		{
			quitEvent = false;
			formatContext = NULL;
			SDL_Init(SDL_INIT_TIMER);
			av_register_all();
		}		

		void Open(char * f)
		{
			filename = f;

			avformat_open_input (&formatContext, filename, NULL, NULL);
			avformat_find_stream_info(formatContext, NULL);
			av_dump_format(formatContext, 0, filename, 0);

			videoStream = getStreamID(AVMEDIA_TYPE_VIDEO);
			audioStream = getStreamID(AVMEDIA_TYPE_AUDIO);
			
			V = new Video(formatContext->streams[videoStream]);
			A = new Audio(formatContext->streams[audioStream]);
			Sync = new Syncer(V, A);
		}

		void Play()
		{
			SDL_AddTimer(10, PushRefreshEvent, V);

			SDL_Thread *demux = SDL_CreateThread(DemuxThread, "demux", this);
			SDL_Thread *video = V->Start();
			// No need to create thread for audio - SDL does this automatically
			A->Start();
			
			StartEventLoop();

			SDL_WaitThread(demux, NULL);
			SDL_WaitThread(video, NULL);
		}			
		
		void Quit()
		{
			quitEvent = true;
			// Tell other threads that they should stop
			A->Quit();
			V->Quit();
		}
		
		~Multimedia()
		{
			avformat_close_input(&formatContext);
			SDL_Quit();
		}
};

int main(int argc, char *argv[])
{
	char * filename;

	if (argc < 2) {
		fprintf(stderr, "Usage: player.exe <file>\n");
		exit(1);
	} else {
		filename = argv[1];
	}

	Multimedia m;
	m.Open(filename);
	m.Play();

	return 0;
}


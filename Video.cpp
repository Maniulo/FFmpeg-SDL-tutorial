#ifndef VIDEO_MY
#define VIDEO_MY

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/mem.h>
	#include <libavutil/time.h>
}

#include <SDL.h>

#include "PacketQueue.cpp"

class Video
{
	private:
		bool			quitEvent;

		SDL_Renderer	*renderer;
		SDL_Texture		*texture;
		SDL_Window		*screen;

		AVCodecContext  *codecContext;
		AVCodec			*codec;
		AVStream		*videoStream;
		PacketQueue		*packetQueue;

		SDL_mutex		*PictureMutex;
		SDL_cond		*PictureReadyCond;
		int				PictureReady;
		
		double clock;

		double UpdateClock(AVFrame* frame)
		{
			if (frame->pkt_dts != AV_NOPTS_VALUE)
				clock = av_q2d(videoStream->time_base) * frame->pkt_dts;
			else if (frame->pkt_pts != AV_NOPTS_VALUE)
				clock = av_q2d(videoStream->time_base) * frame->pkt_pts;
			
			double frame_delay = av_q2d(codecContext->time_base);
			
			/* if we are repeating a frame, adjust clock accordingly */
			frame_delay += frame->repeat_pict * (frame_delay * 0.5);
			clock += frame_delay;

			return clock;
		}

		static int DecodeVideoThread(void *arg)
		{
			Video *v = (Video*)arg;
			v->DecodeVideo();
			return 0;
		}

		int DecodeVideo()
		{
			AVPacket videoPacket;
			AVFrame  frame;
			int frameFinished;

			while (!quitEvent)
			{
				packetQueue->Get(&videoPacket);

				avcodec_decode_video2(codecContext, &frame, &frameFinished, &videoPacket);

				if (frameFinished)
				{
					UpdateClock(&frame);
					PreparePicture(&frame, codecContext);
					av_free_packet(&videoPacket);
				}
			}	
			
			return 0;
		}


		uint8_t * ToYUV420(AVFrame* frame, AVCodecContext *codecContext)
		{
			static int numPixels = avpicture_get_size (PIX_FMT_YUV420P, codecContext->width, codecContext->height);
			static uint8_t * buffer = (uint8_t *) malloc (numPixels);
			
			//Set context for conversion
			static struct SwsContext *swsContext = sws_getCachedContext (
															swsContext,
															codecContext->width,
															codecContext->height,
															codecContext->pix_fmt,
															codecContext->width,
															codecContext->height,
															PIX_FMT_YUV420P,
															SWS_BILINEAR,
															NULL,
															NULL,
															NULL
														);

			AVFrame* frameYUV420P = avcodec_alloc_frame();
			avpicture_fill ((AVPicture *) frameYUV420P, buffer, PIX_FMT_YUV420P, codecContext->width, codecContext->height);

			// Convert the image into YUV format that SDL uses
			sws_scale( swsContext, 
						frame->data, frame->linesize, 
						0, codecContext->height,
						frameYUV420P->data,	frameYUV420P->linesize );

			return buffer;
		}

		void PreparePicture(AVFrame *frame, AVCodecContext *codecContext)
		{
			SDL_LockMutex(PictureMutex);
			while (PictureReady)
				SDL_CondWait(PictureReadyCond, PictureMutex);
			SDL_UnlockMutex(PictureMutex);

			int pitch = codecContext->width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_IYUV);
			uint8_t * buffer = ToYUV420(frame, codecContext);
			SDL_UpdateTexture(texture, NULL, buffer, pitch);

			SDL_LockMutex(PictureMutex);
			PictureReady = true;
			SDL_CondSignal(PictureReadyCond);
			SDL_UnlockMutex(PictureMutex);
		}

	public:

		Video(AVStream *vStream)
		{
			quitEvent = false;

			packetQueue = new PacketQueue();
			SDL_Init(SDL_INIT_VIDEO);

			videoStream = vStream;
			codecContext = videoStream->codec;
			codec = avcodec_find_decoder(codecContext->codec_id);
			avcodec_open2(codecContext, codec, NULL);

			screen = SDL_CreateWindow("My SDL Window",
								  SDL_WINDOWPOS_UNDEFINED,
								  SDL_WINDOWPOS_UNDEFINED,
								  codecContext->width, codecContext->height,
								  SDL_WINDOW_OPENGL);

			renderer = SDL_CreateRenderer(screen, -1, 0);
			texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_IYUV,							// YUV420P
							SDL_TEXTUREACCESS_STREAMING,
							codecContext->width, codecContext->height);

			PictureMutex = SDL_CreateMutex();
			PictureReadyCond = SDL_CreateCond();
			PictureReady = false;
		}

		SDL_Thread * Start()
		{
			return SDL_CreateThread(DecodeVideoThread, "video", this);
		}

		void PutPacket(AVPacket *pkt)
		{
			packetQueue->Put(pkt);
		}
		
		double VideoClock()
		{
			return clock;
		}

		void RenderPicture()
		{
			SDL_LockMutex(PictureMutex);
			while (!PictureReady)
				SDL_CondWait(PictureReadyCond, PictureMutex);
			SDL_UnlockMutex(PictureMutex);

			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);

			SDL_LockMutex(PictureMutex);			
			PictureReady = false;
			SDL_CondSignal(PictureReadyCond);
			SDL_UnlockMutex(PictureMutex);
		}

		void Quit()
		{
			quitEvent = true;
		}			
		
		~Video()
		{
			avcodec_close(videoStream->codec);
		}
};

#endif
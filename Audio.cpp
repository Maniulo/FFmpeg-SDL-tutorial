#ifndef AUDIO_MY
#define AUDIO_MY

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

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000


class Audio
{
	private:
		bool			quitEvent;

		AVCodecContext  *codecContext;
		AVCodec			*codec;
		AVStream		*audioStream;
		PacketQueue		*packetQueue;
		AVPacket		*audioPacket;
		AVFrame			*frame;

		double			clock;
		
		uint8_t			audioBuffer[MAX_AUDIO_FRAME_SIZE];
		unsigned int	audioBufferSize;
		unsigned int	audioBufferIndex;		

		static void PlaybackCallback(void *userdata, Uint8 *stream, int streamSize)
		{
			Audio *a = (Audio*)userdata;
			a->Playback(stream, streamSize);
			return;
		}

		unsigned int dataLeftInBuffer()
		{
			return audioBufferSize - audioBufferIndex;
		}

		void Playback(Uint8 *stream, int streamSize)
		{
			int dataSizeToCopy;

			while (streamSize > 0)
			{
				// Decide how much data the playback stream can hold
				dataSizeToCopy = dataLeftInBuffer();
				if (dataSizeToCopy > streamSize)
					dataSizeToCopy = streamSize;
				
				// Copy decoded data from buffer to the playback stream
				memcpy(stream, (uint8_t *)audioBuffer + audioBufferIndex, dataSizeToCopy);
				streamSize -= dataSizeToCopy;
				stream += dataSizeToCopy;
				audioBufferIndex += dataSizeToCopy;

				if (audioBufferIndex >= audioBufferSize)
				{
					// Already played all decoded data - get more
					audioBufferSize = DecodeAudio(audioBuffer);				
					audioBufferIndex = 0;
				}
			}
		}

		int DecodeAudio(uint8_t *audioBuffer)
		{
			AVPacket audioPacket;
			AVFrame  frame;
			int frameFinished = 0;

			int audioDecodedSize, dataSize = 0;

			while (!quitEvent)
			{				
				packetQueue->Get(&audioPacket);

				audioDecodedSize = avcodec_decode_audio4(codecContext, &frame, &frameFinished, &audioPacket);

				if (frameFinished)
				{
					// Frame loaded from packets - copy it to intermediary buffer
					dataSize = av_samples_get_buffer_size (NULL, codecContext->channels, frame.nb_samples,	codecContext->sample_fmt, 1);
					memcpy(audioBuffer, frame.data[0], dataSize);
					UpdateClock(&audioPacket, dataSize);
					av_free_packet(&audioPacket);
				}

				return dataSize;
			}

			return 0;
		}

		void UpdateClock(AVPacket *packet, int dataSize)
		{
			if (packet->dts != AV_NOPTS_VALUE)	
				clock = av_q2d(audioStream->time_base) * packet->dts;
			else if (frame->pkt_pts != AV_NOPTS_VALUE)
				clock = av_q2d(audioStream->time_base) * frame->pkt_pts;
			else
			{
				// if no pts, then compute it
				clock += (double) dataSize /
								( codecContext->channels * codecContext->sample_rate *
								  av_get_bytes_per_sample(codecContext->sample_fmt)     );
			}
		}

	public:

		Audio(AVStream *aStream)
		{
			quitEvent = false;
			packetQueue = new PacketQueue();
			SDL_Init(SDL_INIT_AUDIO);
			
			audioStream = aStream;

			codecContext = audioStream->codec;
			codec = avcodec_find_decoder(codecContext->codec_id);

			// Hack to play S16P audio - SDL only plays S16
			if (codecContext->sample_fmt == AV_SAMPLE_FMT_S16P)
				codecContext->request_sample_fmt = AV_SAMPLE_FMT_S16;

			avcodec_open2(codecContext, codec, NULL);	

			SDL_AudioSpec desiredSpecs;
			SDL_AudioSpec specs;
			desiredSpecs.freq = codecContext->sample_rate;
			desiredSpecs.format = AUDIO_S16SYS;
			desiredSpecs.channels = codecContext->channels;
			desiredSpecs.silence = 0;
			desiredSpecs.samples = 512;
			desiredSpecs.callback = PlaybackCallback;
			desiredSpecs.userdata = this;
			
			SDL_OpenAudio(&desiredSpecs, &specs);
		}

		void Start()
		{
			SDL_PauseAudio(0);
		}

		void PutPacket(AVPacket *pkt)
		{
			packetQueue->Put(pkt);
		}

		// Audio Clock with account of time to copy and process audio in buffer
		double AudioClock()
		{
			//return clock;
			int bytes_per_sec = codecContext->sample_rate *
									codecContext->channels *
									av_get_bytes_per_sample(codecContext->sample_fmt);
			
			int audioBufferDataSize = audioBufferSize - audioBufferIndex;
	
			if (bytes_per_sec != 0)
				return clock - (double)audioBufferDataSize / bytes_per_sec;
			else
				return clock;
		}

		void Quit()
		{
			quitEvent = true;
		}
		
		~Audio()
		{
			avcodec_close(audioStream->codec);
		}
};

#endif
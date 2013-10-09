#ifndef PACKET_QUEUE
#define PACKET_QUEUE

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/mem.h>
	#include <libavutil/time.h>
}

#include <SDL.h>


class PacketQueue
{
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;

public:
	PacketQueue()
	{
		mutex = SDL_CreateMutex();
		cond = SDL_CreateCond();
		first_pkt = NULL;
		last_pkt = NULL;
	}

	int Put(AVPacket *pkt)
	{
		AVPacketList *newP;
		if (av_dup_packet(pkt) < 0)
			return -1;

		newP = (AVPacketList*) av_malloc(sizeof(AVPacketList));
		newP->pkt = *pkt;
		newP->next = NULL;
  
		SDL_LockMutex(mutex);

		if (!last_pkt)
			first_pkt = newP;
		else
			last_pkt->next = newP;

		last_pkt = newP;
		nb_packets++;
		size += newP->pkt.size;
		SDL_CondSignal(cond);
  
		SDL_UnlockMutex(mutex);
		return 0;
	}

	int Get(AVPacket *pkt)
	{
		AVPacketList *pkt1;
		int ret;

		SDL_LockMutex(mutex);
  
		for(;;)
		{
			pkt1 = first_pkt;
			if (pkt1)
			{
				first_pkt = pkt1->next;
				if (!first_pkt)
				{
					last_pkt = NULL;
				}
				nb_packets--;
				size -= pkt1->pkt.size;
				*pkt = pkt1->pkt;
				av_free(pkt1);
				ret = 1;
				break;
			}
			else
			{
				SDL_CondWait(cond, mutex);
			}
		}
		SDL_UnlockMutex(mutex);

		return ret;
	}
};

#endif
extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/mem.h>
	#include <libavutil/time.h>
}

#include "Video.cpp"
#include "Audio.cpp"

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

#include <SDL.h>

class Syncer
{
	private:
		Audio *A;
		Video *V;
		
		double previousClock;
		double previousDelay;

	public:
		Syncer(Video *v, Audio *a)
		{
			V = v;
			A = a;
			previousClock = 0;
			previousDelay = 0;
		}

		double computeFrameDelay()
		{
//			fprintf(stdout, "%f", V->VideoClock());
			double delay = V->VideoClock() - previousClock;
			if (delay <= 0.0 || delay >= 1.0) {
				// Incorrect delay - use previous one
				delay = previousDelay;
			}
			// Save for next time
			previousClock = V->VideoClock();
			previousDelay = delay;

			// Update delay to sync to audio
			double diff = V->VideoClock() - A->AudioClock();

			if (diff <= -delay)		delay = 0;				// Audio is ahead of video; display video ASAP
			if (diff >=  delay)		delay = 2 * delay;		// Video is ahead of audio; delay video				

			return delay;
		}

};
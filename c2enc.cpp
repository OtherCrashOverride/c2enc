#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include <exception>
#include <vector>
#include <cstring>
#include <memory>

#include "../c2_vpcodec/include/vpcodec_1_0.h"


const int DEFAULT_BITRATE = 1000000 * 5;


struct option longopts[] = {
	{ "width",			required_argument,	NULL,	'w' },
	{ "height",			required_argument,	NULL,	'h' },
	{ "fps",			required_argument,	NULL,	'f' },
	{ "bitrate",		required_argument,	NULL,	'b' },
	{ "gop",			required_argument,	NULL,	'g' },
	{ 0, 0, 0, 0 }
};

class Exception : public std::exception
{
public:
	Exception(const char* message)
		: std::exception()
	{
		fprintf(stderr, "%s\n", message);
	}

};


int main(int argc, char** argv)
{
	int io;


	// options
	int width = -1;
	int height = -1;
	int framerate = -1;
	int bitrate = DEFAULT_BITRATE;
	int gop = 10;


	int c;
	while ((c = getopt_long(argc, argv, "w:h:f:b:g:", longopts, NULL)) != -1)
	{
		switch (c)
		{
			case 'w':
				width = atoi(optarg);
				break;

			case 'h':
				height = atoi(optarg);
				break;

			case 'f':
				framerate = atoi(optarg);
				break;

			case 'b':
				bitrate = atoi(optarg);
				break;

			case 'g':
				gop = atoi(optarg);
				break;

			default:
				throw Exception("Unknown option.");
		}
	}


	if (width == -1 || height == -1 ||
		framerate == -1)
	{
		throw Exception("Required parameter missing.");
	}



	// Initialize the encoder

	fprintf(stderr, "vl_video_encoder_init: width=%d, height=%d, frame_rate=%d, bit_rate=%d, gop=%d\n",
		width, height, framerate, bitrate, gop);

	vl_codec_id_t codec_id = CODEC_ID_H264;
	vl_img_format_t img_format = IMG_FMT_NV12;
	vl_codec_handle_t handle = vl_video_encoder_init(codec_id, width, height, framerate, bitrate, gop, img_format);
	fprintf(stderr, "handle = %ld\n", handle);



	// Start streaming
	int frameCount = 0;

	// TODO: smart pointer
	int bufferSize = width * height;	// Y	
	bufferSize += bufferSize / 2;	// U, V

	unsigned char* input = new unsigned char[bufferSize];

	const int outputBufferSize = width * height * 4;
	char* outputBuffer = new char[outputBufferSize];

	while (true)
	{
		// get buffer
		ssize_t totalRead = 0;
		while (totalRead < bufferSize)
		{
			ssize_t readCount = read(STDIN_FILENO, input + totalRead, bufferSize - totalRead);
			if (readCount <= 0)
			{
				//throw Exception("read failed.");

				// End of stream?
				fprintf(stderr, "read failed. (%ld)\n", readCount);
				break;
			}
			else
			{
				totalRead += readCount;
			}
		}

		if (totalRead < bufferSize)
		{
			fprintf(stderr, "read underflow. (%ld of %d)\n", totalRead, bufferSize);
			break;
		}


		// Encode the video frames
		vl_frame_type_t type = FRAME_TYPE_AUTO;
		char* in = (char*)input;
		int in_size = bufferSize;
		char* out = outputBuffer;
		int outCnt = vl_video_encoder_encode(handle, type, in, in_size, &out);
		
		if (outCnt > outputBufferSize)
		{
			throw Exception("Output buffer overflow.");
		}

		if (outCnt > 0)
		{
			ssize_t writeCount = write(STDOUT_FILENO, out, outCnt);
			if (writeCount < 0)
			{
				throw Exception("write failed.");
			}
		}


		// Stats
		if ((frameCount % 100) == 0)
		{
			fprintf(stderr, "frameCount=%d.\n", frameCount);
		}

		++frameCount;
	}


	// Close the decoder
	vl_video_encoder_destory(handle);

	return 0;
}


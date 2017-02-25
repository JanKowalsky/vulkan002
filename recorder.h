#ifndef RECORDER_H
#define RECORDER_H

#include <string>
#include <cstdint>

extern "C"{
#include <x264.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}

class VulkanRecorder
{
public:
	VulkanRecorder() = default;
	~VulkanRecorder() = default;
	
	void startRecording(std::string filename, uint16_t in_res_x, uint16_t in_res_y, uint16_t out_res_x, uint16_t out_res_y, uint8_t fps);
	void nextFrame(uint8_t* rgba);
	void stopRecording();
	
private:
	bool open(std::string);
	bool encode(uint8_t*);
	bool close();
	void setParams();
	bool validateSettings();
	
	/*user specified*/
	int m_width_in = 0;
	int m_height_in = 0;
	int m_width_out = 0;
	int m_height_out = 0;
	int m_fps = 25;
	AVPixelFormat m_pixel_format_in = AV_PIX_FMT_RGBA;
	AVPixelFormat m_pixel_format_out = AV_PIX_FMT_YUV420P;

	/* x264 */
	AVPicture m_pic_raw;
	x264_picture_t m_pic_in;
	x264_picture_t m_pic_out;
	x264_param_t m_params;
	x264_nal_t* m_nals;
	x264_t* m_encoder = NULL;
	int m_num_nals = 0;

	/* input / output */
	int m_pts = 0;
	SwsContext* m_sws = NULL;
	FILE* m_file = NULL;
};

#endif //RECORDER_H
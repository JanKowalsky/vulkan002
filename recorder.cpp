#include "recorder.h"
#include <iostream>

using namespace std;

void ErrorMessage(const char* msg)
{
	cout << msg << endl;
}

void VulkanRecorder::setParams()
{
	x264_param_default_preset(&m_params, "ultrafast", "zerolatency");
	m_params.i_threads = 1;
	m_params.i_width = m_width_out;
	m_params.i_height = m_height_out;
	m_params.i_fps_num = m_fps;
	m_params.i_fps_den = 1;
}

bool VulkanRecorder::validateSettings()
{
	if(!m_width_in)
	{
		ErrorMessage("No in_width set");
		return false;
	}
	if(!m_height_in)
	{
		ErrorMessage("No in_height set");
		return false;
	}
	if(!m_width_out)
	{
		ErrorMessage("No out_width set");
		return false;
	}
	if(!m_height_out)
	{
		ErrorMessage("No out_height set");
		return false;
	}
	if(m_pixel_format_in == AV_PIX_FMT_NONE)
	{
		ErrorMessage("No in_pixel_format set");
		return false;
	}
	if(m_pixel_format_out == AV_PIX_FMT_NONE)
	{
		ErrorMessage("No out_pixel_format set");
		return false;
	}
	return true;
}

bool VulkanRecorder::close()
{
	if(m_encoder)
	{
		x264_picture_clean(&m_pic_in);
		memset((char*)&m_pic_in, 0, sizeof(m_pic_in));
		memset((char*)&m_pic_out, 0, sizeof(m_pic_out));
		
		x264_encoder_close(m_encoder);
		m_encoder = NULL;
	}
	
	if(m_sws)
	{
		sws_freeContext(m_sws);
		m_sws = NULL;
	}
	
	memset((char*)&m_pic_raw, 0, sizeof(m_pic_raw));
	
	if(m_file)
	{
		fclose(m_file);
		m_file = NULL;
	}
	return true;
}

bool VulkanRecorder::open(std::string filename)
{
	if(!validateSettings())
	{
		return false;
	}
	
	int r = 0;
	int nheader = 0;
	int header_size = 0;
	
	if(m_encoder)
	{
		ErrorMessage("Already opened. first call close()");
		return false;
	}
	
	if(m_pixel_format_out != AV_PIX_FMT_YUV420P)
	{
		ErrorMessage("At this moment the output format must be AV_PIX_FMT_YUV420P");
		return false;
	}
	
	m_sws = sws_getContext(m_width_in, m_height_in, m_pixel_format_in,
						m_width_out, m_height_out, m_pixel_format_out,
						SWS_FAST_BILINEAR, NULL, NULL, NULL);
	
	if(!m_sws)
	{
		ErrorMessage("Cannot create SWS context");
		return false;
	}
	
	m_file = fopen(filename.c_str(), "w+b");
	if(!m_file)
	{
		ErrorMessage("Cannot open the h264 destination file");
		goto error;
	}
	
	
	x264_picture_alloc(&m_pic_in, X264_CSP_I420, m_width_out, m_height_out);
	
	setParams();
	
	m_encoder = x264_encoder_open(&m_params);
	if(!m_encoder)
	{
		ErrorMessage("Cannot open the encoder");
		goto error;
	}
	
	/*write headers*/
	r = x264_encoder_headers(m_encoder, &m_nals, &nheader);
	if(r < 0)
	{
		ErrorMessage("x264_encoder_headers() failed");
		goto error;
	}
	
	header_size = m_nals[0].i_payload + m_nals[1].i_payload +m_nals[2].i_payload;
	if(!fwrite(m_nals[0].p_payload, header_size, 1, m_file))
	{
		ErrorMessage("Cannot write headers");
		goto error;
	}
	
	m_pts = 0;
	
	return true;
	
	error:
	close();
	return false;
}

bool VulkanRecorder::encode(uint8_t* pixels)
{
	if(!m_sws)
	{
		ErrorMessage("Not initialized, so cannot encode");
		return false;
	}
	
	/*copy the pixels into "raw input" container*/
	int bytes_filled = avpicture_fill(&m_pic_raw, (uint8_t*)pixels, m_pixel_format_in, m_width_in, m_height_in);
	if(!bytes_filled)
	{
		ErrorMessage("Cannot fill the raw input buffer");
		return false;
	}
	
	/*convert to I420 for x264*/
	int h = sws_scale(m_sws, m_pic_raw.data, m_pic_raw.linesize, 0, m_height_in, m_pic_in.img.plane, m_pic_in.img.i_stride);
	
	if(h != m_height_out) {
		ErrorMessage("scale failed");
		return false;
	}
	
	/*and encode and store into pic_out*/
	m_pic_in.i_pts = m_pts;
	
	int frame_size = x264_encoder_encode(m_encoder, &m_nals, &m_num_nals, &m_pic_in, &m_pic_out);
	if(frame_size) {
		if(!fwrite(m_nals[0].p_payload, frame_size, 1, m_file)) {
		ErrorMessage("Error while trying to write nal");
		return false;
		}
	}
	m_pts++;
	
	return true;
}

void VulkanRecorder::startRecording(std::string filename, uint16_t in_res_x, uint16_t in_res_y, uint16_t out_res_x, uint16_t out_res_y, uint8_t fps)
{
	/*zero out picture*/
	memset((char*)&m_pic_raw, 0, sizeof(m_pic_raw));
	
	m_width_in = in_res_x;
	m_height_in = in_res_y;
	m_width_out = out_res_x;
	m_height_out = out_res_y;
	m_fps = fps;
	
	open(filename);
}

void VulkanRecorder::nextFrame(uint8_t* rgba)
{
	encode(rgba);
}

void VulkanRecorder::stopRecording()
{
	if(m_sws)
		close();
}
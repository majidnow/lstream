#include "LightStream.h"
#include "crc.h"

#ifdef MCU
#include "string.h"
#include "utils.h"
#endif

#define START_BYTE_CAT  0x41
#define START_BYTE_DOG  0x24

#define READ_BUFFER_SIZE	(MAX_DATA_SIZE+7)
#define WRITE_BUFFER_SIZE	(MAX_DATA_SIZE+7)

#define FRAME_STEP_CAT      ((uint8_t)0)
#define FRAME_STEP_DOG      ((uint8_t)1)
#define FRAME_STEP_LEN1     ((uint8_t)2)
#define FRAME_STEP_LEN2     ((uint8_t)3)
#define FRAME_STEP_TYPE     ((uint8_t)4)
#define FRAME_STEP_DATA     ((uint8_t)5)

LightStream::LightStream(std::shared_ptr<IFrame> upper)
    :
    upper(upper),
    frame(),
	rbuffer(new uint8_t[READ_BUFFER_SIZE]),
	rposition(0),
	wbuff(WRITE_BUFFER_SIZE)
{
    Reset();
    //crcInit();
}

uint8_t* LightStream::ReadBuffer()
{
	return rbuffer + rposition;
}

LSBuff& LightStream::WriteBuffer()
{
	return wbuff;
}

size_t LightStream::Size()
{
	return READ_BUFFER_SIZE - rposition;
}

void LightStream::Check(size_t nread)
{
    uint8_t *buff = rbuffer + rposition;
    int index = 0;
    while (nread > index)
    {
        if (frame.step < FRAME_STEP_DATA)
        {
            for (; nread > index; index++)
            {
                if (frame.step == FRAME_STEP_CAT && buff[index] == START_BYTE_CAT)
                {
                    // when setp == 0, cat found, looking for dog
                    frame.step++;
                }
                else if (frame.step == FRAME_STEP_DOG && buff[index] == START_BYTE_DOG)
                {
                    // when step == 1, dog found too, we have a frame
                    frame.step++;
                }
                else if (frame.step == FRAME_STEP_LEN1)
                {
                    // we calculating crc from whole of data except start bytes
                    // so we store current position 
                    frame.buffer = &buff[index];
                    frame.msg_len = buff[index];
                    frame.step++;
                }
                else if (frame.step == FRAME_STEP_LEN2)
                {
                    frame.msg_len |= ((uint16_t)buff[index]) << 8;
                    frame.length = frame.msg_len;
                    // add crc length to msg_len
                    frame.msg_len += 2;
                    frame.step++;
                }
                else if (frame.step == FRAME_STEP_TYPE)
                {
                    frame.msg_type = buff[index];
                    frame.step++;
                    // when loop break we need to increase index manually
                    index++;
                    break;
                }
                // we need to reset frame tracking status only when we started tracking (step > 0)
                else if (frame.step)
                    Reset();
            }
        }

        // fetch data body
        if (frame.step == FRAME_STEP_DATA)
        {
            // available data
            uint16_t a = nread - index;
            // remained data
            uint16_t r = frame.msg_len - frame.nread;

            if (a >= r)
            {
                frame.nread += r;
                index += r;
            }
            else
            {
                frame.nread += a;
                index += a;
            }
            // we got whole data next step is getting crc
            if (frame.nread == frame.msg_len)
            {
                uint8_t* temp_buff;
                uint16_t crc;
                if (frame.first_piece_len == 0)
                {
                    temp_buff = frame.buffer;
                    crc = *((uint16_t*)(buff + index - 2));
                }
                else
                {
                    temp_buff = new uint8_t[frame.length + 5];
                    memcpy(temp_buff, frame.buffer, frame.first_piece_len);
                    memcpy(temp_buff + frame.first_piece_len, rbuffer, frame.nread-frame.first_piece_len+3);
                    crc = *((uint16_t*)(temp_buff+frame.length+3));
                }

                //for (int i = 0; i < frame.length + 3; i++)
                //    std::cout << std::hex << (int)temp_buff[i] << " ";
                //std::cout << std::endl;

                // we have 1 byte for type and 2 bytes for length so we add 3 to msg_len 
                uint16_t cal_crc = crcFast(temp_buff, frame.length + 3);
                if (cal_crc == crc)
                {
                    frame.buffer = temp_buff + 3;
                    frame.status = Status::OK;
                    frame.type = (FrameType)frame.msg_type;
                }
                else
                    frame.status = Status::CRC_ERROR;

                upper->OnFrame(frame);
                if (frame.first_piece_len > 0)
                    delete[] temp_buff;
                Reset();
            }
        }
    }

    // reset buffer position
    rposition += nread;
    // if circular buffer overflowed
    if (rposition >= READ_BUFFER_SIZE)
    {
        // the position need to set zero
        rposition = 0;
        // seeking frame data body started the so data will be turned to pieces
        if (frame.step >= FRAME_STEP_TYPE)
            frame.first_piece_len = frame.nread+(frame.step-2);
    }
}

void LightStream::Reset()
{
    frame.nread = 0;
    frame.step = 0;
    frame.msg_len = 0;
    frame.msg_type = 0;
    frame.first_piece_len = 0;
}

void LightStream::InitFrame()
{
	wbuff.ptr[0] = START_BYTE_CAT;
	wbuff.ptr[1] = START_BYTE_DOG;
	wbuff.position = 5;
}

void LightStream::FrameHeader(FrameType type, size_t dsize)
{
	memcpy(wbuff.ptr + 2, &dsize, 2);
	wbuff.ptr[4] = (uint8_t)type;
}

void LightStream::PushDataToFrame(uint8_t* data, size_t size)
{
	memcpy(wbuff.End(), data, size);
	wbuff.position += size;
}

LSBuff* LightStream::Frame()
{
	uint16_t crc = crcFast(wbuff.ptr + 2, wbuff.position-2);
	memcpy(wbuff.End(), &crc, 2);
	wbuff.position += 2;

	return &wbuff;
}


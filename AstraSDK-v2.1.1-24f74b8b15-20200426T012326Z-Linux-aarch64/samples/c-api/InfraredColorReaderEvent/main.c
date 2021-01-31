// This file is part of the Orbbec Astra SDK [https://orbbec3d.com]
// Copyright (c) 2015-2017 Orbbec 3D
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Be excellent to each other.
#include <astra/capi/astra.h>
#include <astra/capi/streams/infrared_capi.h>
#include <stdio.h>
#include <key_handler.h>


int count = 0;

void print_infrared(astra_infraredframe_t infraredFrame)
{
    astra_image_metadata_t metadata;

    uint32_t irLength;

    uint8_t* dataBytes = NULL;
    astra_infraredframe_get_data_ptr(infraredFrame, &dataBytes, &irLength);
    astra_infraredframe_get_metadata(infraredFrame, &metadata);

    uint16_t* irData = (uint16_t*)dataBytes;

    int width = metadata.width;
    int height = metadata.height;

    size_t index = ((width * (height / 2)) + (width / 2));
    uint16_t middle = irData[index];

    astra_frame_index_t frameIndex;
    astra_infraredframe_get_frameindex(infraredFrame, &frameIndex);

    printf("infrared frameIndex:  %d  value:  %d \n", frameIndex, middle);
}

void print_color(astra_colorframe_t colorFrame)
{
    astra_image_metadata_t metadata;
    astra_rgb_pixel_t* colorData_rgb;

    uint32_t colorByteLength;

    astra_colorframe_get_data_rgb_ptr(colorFrame, &colorData_rgb, &colorByteLength);

    astra_colorframe_get_metadata(colorFrame, &metadata);

    int width = metadata.width;
    int height = metadata.height;
    size_t index = ((width * (height / 2)) + (width / 2));

    astra_frame_index_t frameIndex;
    astra_colorframe_get_frameindex(colorFrame, &frameIndex);

    astra_rgb_pixel_t middle = colorData_rgb[index];
    printf("color frameIndex: %d  r: %d    g: %d    b: %d \n", frameIndex, (int)(middle.r), (int)(middle.g), (int)(middle.b));
}

void frame_ready(void* clientTag, astra_reader_t reader, astra_reader_frame_t frame)
{
    astra_infraredframe_t infraredFrame;
    astra_frame_get_infraredframe(frame, &infraredFrame);
    //Check for NULL because we reuse this method for both infrared and color events
    if (infraredFrame != NULL)
    {
        print_infrared(infraredFrame);
    }

    astra_colorframe_t colorFrame;
    astra_frame_get_colorframe(frame, &colorFrame);
    if (colorFrame != NULL)
    {
        print_color(colorFrame);
    }

    if (infraredFrame != NULL || colorFrame != NULL)
    {
        //clientTag is pointer to frameCount, pass to astra_reader_register_frame_ready_callback()
        int* frameCount = (int*)clientTag;
        //Increment frameCount only when we have a succesful frame
        (*frameCount)++;
    }
}

astra_infraredstream_t setup_infrared(astra_reader_t reader)
{
    astra_infraredstream_t infraredStream;
    astra_reader_get_infraredstream(reader, &infraredStream);

    astra_imagestream_mode_t mode;
    //640x480@30 or 1280x1024@30
    //(Infrared 1280x1024 will only produce at ~5fps)
    mode.width = 640;
    mode.height = 480;
    //IR supports ASTRA_PIXEL_FORMAT_RGB888 and ASTRA_PIXEL_FORMAT_GRAY16.
    //If you change here, make sure to change the code in print_infrared to handle
    //the proper format:
    //ASTRA_PIXEL_FORMAT_RGB888 data = uint8t[], length = width*height*3 (RGB per pixel)
    //ASTRA_PIXEL_FORMAT_GRAY16 data = int16_t[], length = width*height*2 (IR intensity)
    mode.pixelFormat = ASTRA_PIXEL_FORMAT_GRAY16;
    mode.fps = 30;
    astra_imagestream_set_mode(infraredStream, &mode);

    return infraredStream;
}

astra_colorstream_t setup_color(astra_reader_t reader)
{
    astra_colorstream_t colorStream;
    astra_reader_get_colorstream(reader, &colorStream);

    astra_imagestream_mode_t mode;
    //640x480@30 or 1280x1024@30
    //(Color 1280x1024 will only produce at ~5fps)
    mode.width = 640;
    mode.height = 480;
    mode.pixelFormat = ASTRA_PIXEL_FORMAT_RGB888;
    mode.fps = 30;
    astra_imagestream_set_mode(colorStream, &mode);

    return colorStream;
}

int main(int argc, char* argv[])
{
    set_key_handler();

    astra_initialize();

    astra_streamsetconnection_t sensor;

    astra_streamset_open("device/default", &sensor);

    astra_reader_t reader;
    astra_reader_create(sensor, &reader);

    astra_infraredstream_t infraredStream = setup_infrared(reader);
    astra_colorstream_t colorStream = setup_color(reader);

    bool infraredOn = true;
    astra_stream_start(infraredStream);

    int frameCount = 0;

    astra_reader_callback_id_t callbackId;
    astra_reader_register_frame_ready_callback(reader, &frame_ready, &frameCount, &callbackId);

    do
    {
        astra_update();

        if (frameCount >= 30)
        {
            frameCount = 0;
            if (infraredOn)
            {
                infraredOn = false;
                printf("Switching to color stream...\n");
                //Must first stop infrared then we can start color
                astra_stream_stop(infraredStream);
                astra_stream_start(colorStream);
            }
            else
            {
                infraredOn = true;
                printf("Switching to infrared stream...\n");
                //Must first stop color then we can start infrared
                astra_stream_stop(colorStream);
                astra_stream_start(infraredStream);
            }
        }
    } while (shouldContinue);

    astra_reader_unregister_frame_ready_callback(&callbackId);

    astra_reader_destroy(&reader);
    astra_streamset_close(&sensor);

    astra_terminate();
}

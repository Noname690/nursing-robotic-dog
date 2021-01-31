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

void frame_ready(void* clientTag, astra_reader_t reader, astra_reader_frame_t frame)
{
    astra_infraredframe_t infraredFrame;
    astra_frame_get_infraredframe(frame, &infraredFrame);

    print_infrared(infraredFrame);
}

int main(int argc, char* argv[])
{
    set_key_handler();

    astra_initialize();

    astra_streamsetconnection_t sensor;

    astra_streamset_open("device/default", &sensor);

    astra_reader_t reader;
    astra_reader_create(sensor, &reader);

    astra_infraredstream_t infraredStream;
    astra_reader_get_infraredstream(reader, &infraredStream);

    astra_imagestream_mode_t mode;
    mode.width = 640;
    mode.height = 480;
    mode.pixelFormat = ASTRA_PIXEL_FORMAT_GRAY16;
    mode.fps = 30;
    astra_imagestream_set_mode(infraredStream, &mode);

    astra_stream_start(infraredStream);

    astra_reader_callback_id_t callbackId;
    astra_reader_register_frame_ready_callback(reader, &frame_ready, NULL, &callbackId);

    do
    {
        astra_update();
    } while (shouldContinue);

    astra_reader_unregister_frame_ready_callback(&callbackId);

    astra_reader_destroy(&reader);
    astra_streamset_close(&sensor);

    astra_terminate();
}

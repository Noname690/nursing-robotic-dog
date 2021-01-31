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
#include <stdio.h>
#include <key_handler.h>
#include <astra_core/capi/astra_types.h>

void print_depth(astra_depthframe_t depthFrame)
{
    astra_image_metadata_t metadata;

    int16_t* depthData;
    uint32_t depthLength;

    astra_depthframe_get_data_byte_length(depthFrame, &depthLength);
    astra_depthframe_get_metadata(depthFrame, &metadata);

    depthData = (int16_t*)malloc(depthLength);
    astra_depthframe_copy_data(depthFrame, depthData);

    int width = metadata.width;
    int height = metadata.height;

    size_t index = ((width * (height / 2)) + (width / 2));
    short middle = depthData[index];

    free(depthData);

    astra_frame_index_t frameIndex;
    astra_depthframe_get_frameindex(depthFrame, &frameIndex);
    printf("depth frameIndex %d value %d\n", frameIndex, middle);
}

int main(int argc, char* argv[])
{
    set_key_handler();

    astra_initialize();

    astra_streamsetconnection_t sensor;

    astra_streamset_open("device/default", &sensor);

    astra_reader_t reader;
    astra_reader_create(sensor, &reader);

    astra_depthstream_t depthStream;
    astra_reader_get_depthstream(reader, &depthStream);

    float hFov, vFov;
    astra_depthstream_get_hfov(depthStream, &hFov);
    astra_depthstream_get_vfov(depthStream, &vFov);
    printf("depth sensor -- hFov: %f radians vFov: %f radians\n", hFov, vFov);

    char serialnumber[128];
    astra_depthstream_get_serialnumber(depthStream,serialnumber,128);
    printf("depth sensor -- serial number: %s\n", serialnumber);

    const uint32_t chipId;
    astra_depthstream_get_chip_id(depthStream, &chipId);

    switch (chipId)
    {
        case ASTRA_CHIP_ID_MX400:
            printf("Chip ID: MX400\n");
            break;
        case ASTRA_CHIP_ID_MX6000:
            printf("Chip ID: MX6000\n");
            break;
        case ASTRA_CHIP_ID_UNKNOWN:
        default:
            printf("Chip ID: Unknown\n");
            break;
    }

    astra_stream_start(depthStream);

    astra_frame_index_t lastFrameIndex = -1;

    do
    {
        astra_update();

        astra_reader_frame_t frame;
        astra_status_t rc = astra_reader_open_frame(reader, 0, &frame);

        if (rc == ASTRA_STATUS_SUCCESS)
        {
            astra_depthframe_t depthFrame;
            astra_frame_get_depthframe(frame, &depthFrame);

            astra_frame_index_t newFrameIndex;
            astra_depthframe_get_frameindex(depthFrame, &newFrameIndex);

            if (lastFrameIndex == newFrameIndex)
            {
                printf("duplicate frame index: %d\n", lastFrameIndex);
            }
            lastFrameIndex = newFrameIndex;

            print_depth(depthFrame);

            astra_reader_close_frame(&frame);
        }

    } while (shouldContinue);

    astra_reader_destroy(&reader);
    astra_streamset_close(&sensor);

    astra_terminate();
}

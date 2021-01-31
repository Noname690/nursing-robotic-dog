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
#include <SFML/Graphics.hpp>
#include <astra/astra.hpp>
#include "LitDepthVisualizer.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <key_handler.h>
#include <sstream>

class MaskedColorFrameListener : public astra::FrameListener
{
public:
    MaskedColorFrameListener(const astra::CoordinateMapper& coordinateMapper)
        : coordinateMapper_(coordinateMapper)
    {
        prev_ = ClockType::now();
        font_.loadFromFile("Inconsolata.otf");
    }

    void ensure_texture(int width, int height)
    {
        if (!displayBuffer_ ||
            width != displayWidth_ ||
            height != displayHeight_)
        {
            displayWidth_ = width;
            displayHeight_ = height;
            textureBytesPerPixel_ = 4; // RGBA

            const int byteLength = displayWidth_ * displayHeight_ * textureBytesPerPixel_;

            displayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::fill(&displayBuffer_[0], &displayBuffer_[0] + byteLength, 0);

            texture_.create(displayWidth_, displayHeight_);
            sprite_.setTexture(texture_, true);
            sprite_.setPosition(0, 0);
        }
    }

    void check_fps()
    {
        const float frameWeight = .2f;

        const ClockType::time_point now = ClockType::now();
        const float elapsedMillis = std::chrono::duration_cast<DurationType>(now - prev_).count();

        elapsedMillis_ = elapsedMillis * frameWeight + elapsedMillis_ * (1.f - frameWeight);
        prev_ = now;

        const float fps = 1000.f / elapsedMillis;

        const auto precision = std::cout.precision();

        std::cout << std::fixed
                  << std::setprecision(1)
                  << fps << " fps ("
                  << std::setprecision(1)
                  << elapsedMillis_ << " ms)"
                  << std::setprecision(precision)
                  << std::endl;
    }

    void on_frame_ready(astra::StreamReader& reader,
                        astra::Frame& frame) override
    {
        const astra::MaskedColorFrame MaskedColorFrame = frame.get<astra::MaskedColorFrame>();

        const int maskedColorWidth = MaskedColorFrame.width();
        const int maskedColorHeight = MaskedColorFrame.height();

        if (maskedColorWidth <= 0 || maskedColorHeight <= 0) { return; }

        ensure_texture(maskedColorWidth, maskedColorHeight);

        check_fps();

        if (isPaused_) { return; }

        copy_depth_data(frame);

        const float invScaleX = depthWidth_ / float(maskedColorWidth);
        /*when bodyMask is 640*400 or colorFrame is 1280*720, scaleY still equals to scaleX.*/
        const float invScaleY = invScaleX; //depthHeight_ == 400 ? invScaleX : (depthHeight_ / float(maskedColorHeight));


        const astra::RgbaPixel* srcData = MaskedColorFrame.data();

        for (int y = 0; y <  maskedColorHeight; y++)
        {
            const int depthY = static_cast<int>(y * invScaleY);

            for (int x = 0; x < maskedColorWidth; x++)
            {
                const int srcIndex = y * maskedColorWidth + x;
                const int destIndex = srcIndex * textureBytesPerPixel_;

                if (srcData[srcIndex].alpha != 0)
                {
                    displayBuffer_[destIndex]     = srcData[srcIndex].r;
                    displayBuffer_[destIndex + 1] = srcData[srcIndex].g;
                    displayBuffer_[destIndex + 2] = srcData[srcIndex].b;
                    displayBuffer_[destIndex + 3] = srcData[srcIndex].alpha;
                }
                else
                {
                    const int depthX = static_cast<int>(x * invScaleX);
                    const int depthIndex = depthY * depthWidth_ + depthX;

                    displayBuffer_[destIndex]     = uint8_t(depthData_[depthIndex]);
                    displayBuffer_[destIndex + 1] = uint8_t(depthData_[depthIndex]);
                    displayBuffer_[destIndex + 2] = uint8_t(depthData_[depthIndex]);
                    displayBuffer_[destIndex + 3] = 255; // Fully opaque

                }
            }
        }

        texture_.update(displayBuffer_.get());
    }

    void copy_depth_data(astra::Frame& frame)
    {
        const astra::DepthFrame depthFrame = frame.get<astra::DepthFrame>();

        if (depthFrame.is_valid())
        {
            const int width = depthFrame.width();
            const int height = depthFrame.height();
            if (!depthData_ || width != depthWidth_ || height != depthHeight_)
            {
                depthWidth_ = width;
                depthHeight_ = height;

                // texture is RGBA
                const int byteLength = depthWidth_ * depthHeight_ * sizeof(uint16_t);

                depthData_ = DepthPtr(new int16_t[byteLength]);
            }

            depthFrame.copy_to(&depthData_[0]);
        }
    }

    void update_mouse_position(sf::RenderWindow& window)
    {
        const sf::Vector2i position = sf::Mouse::getPosition(window);
        const sf::Vector2u windowSize = window.getSize();

        mouseNormX_ = position.x / float(windowSize.x);
        mouseNormY_ = position.y / float(windowSize.y);
    }

    void draw_text(sf::RenderWindow& window,
                   sf::Text& text,
                   sf::Color color,
                   const int x,
                   const int y) const
    {
        text.setColor(sf::Color::Black);
        text.setPosition(x + 5, y + 5);
        window.draw(text);

        text.setColor(color);
        text.setPosition(x, y);
        window.draw(text);
    }

    void draw_mouse_overlay(sf::RenderWindow& window,
                            const float depthWScale,
                            const float depthHScale) const
    {
        if (!isMouseOverlayEnabled_ || !depthData_) { return; }

        const int mouseX = depthWidth_ * mouseNormX_;
        const int mouseY = depthHeight_ * (depthHeight_ == 400 ? mouseNormY_*1.2 : mouseNormY_);

        if (mouseX >= depthWidth_ ||
            mouseY >= depthHeight_ ||
            mouseX < 0 ||
            mouseY < 0) { return; }

        const size_t index = (depthWidth_ * mouseY + mouseX);
        const short z = depthData_[index];

        float worldX, worldY, worldZ;
        coordinateMapper_.convert_depth_to_world(float(mouseX),
                                                 float(mouseY),
                                                 float(z),
                                                 worldX,
                                                 worldY,
                                                 worldZ);

        std::stringstream str;
        str << std::fixed
            << std::setprecision(0)
            << "(" << mouseX << ", " << mouseY << ") "
            << "X: " << worldX << " Y: " << worldY << " Z: " << worldZ;

        const int characterSize = 40;
        sf::Text text(str.str(), font_);
        text.setCharacterSize(characterSize);
        text.setStyle(sf::Text::Bold);

        const float displayX = 10.f;
        const float margin = 10.f;
        const float displayY = window.getView().getSize().y - (margin + characterSize);

        draw_text(window, text, sf::Color::White, displayX, displayY);
    }

    void draw_help_message(sf::RenderWindow& window) const
    {
        if (!isMouseOverlayEnabled_) {
            return;
        }

        std::stringstream str;
        str << "press h to toggle help message";

        if (isFullHelpEnabled_ && helpMessage_ != nullptr)
        {
            str << "\n" << helpMessage_;
        }

        const int characterSize = 30;
        sf::Text text(str.str(), font_);
        text.setCharacterSize(characterSize);
        text.setStyle(sf::Text::Bold);

        const float displayX = 0.f;
        const float displayY = 0;

        draw_text(window, text, sf::Color::White, displayX, displayY);
    }

    void draw_to(sf::RenderWindow& window)
    {
        if (displayBuffer_ != nullptr)
        {
            const float depthWScale = window.getView().getSize().x / displayWidth_;
            const float depthHScale = window.getView().getSize().y / displayHeight_;

            sprite_.setScale(depthWScale, depthHScale);
            window.draw(sprite_);

            draw_mouse_overlay(window, depthWScale, depthHScale);
            draw_help_message(window);
        }
    }

    void toggle_paused()
    {
        isPaused_ = !isPaused_;
    }

    bool is_paused() const
    {
        return isPaused_;
    }

    void toggle_overlay()
    {
        isMouseOverlayEnabled_ = !isMouseOverlayEnabled_;
    }

    bool overlay_enabled() const
    {
        return isMouseOverlayEnabled_;
    }

    void toggle_help()
    {
        isFullHelpEnabled_ = !isFullHelpEnabled_;
    }

    void set_help_message(const char* msg)
    {
        helpMessage_ = msg;
    }

private:
    samples::common::LitDepthVisualizer visualizer_;

    using DurationType = std::chrono::milliseconds;
    using ClockType = std::chrono::high_resolution_clock;

    ClockType::time_point prev_;
    float elapsedMillis_{.0f};

    int textureBytesPerPixel_{0};
    sf::Texture texture_;
    sf::Sprite sprite_;
    sf::Font font_;

    const astra::CoordinateMapper& coordinateMapper_;

    int displayWidth_{0};
    int displayHeight_{0};

    using BufferPtr = std::unique_ptr<uint8_t[]>;
    BufferPtr displayBuffer_{nullptr};

    int depthWidth_{0};
    int depthHeight_{0};

    using DepthPtr = std::unique_ptr<int16_t[]>;
    DepthPtr depthData_{nullptr};

    float mouseNormX_{0};
    float mouseNormY_{0};
    bool isPaused_{false};
    bool isMouseOverlayEnabled_{true};
    bool isFullHelpEnabled_{false};
    const char* helpMessage_{nullptr};
};

astra::DepthStream configure_depth(astra::StreamReader& reader)
{
    auto depthStream = reader.stream<astra::DepthStream>();

    //We don't have to set the mode to start the stream, but if you want to here is how:
    astra::ImageStreamMode depthMode;

    depthMode.set_width(640);
    depthMode.set_height(480);
    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
    depthMode.set_fps(30);

    depthStream.set_mode(depthMode);

    return depthStream;
}

astra::ColorStream configure_color(astra::StreamReader& reader)
{
    auto colorStream = reader.stream<astra::ColorStream>();

    //We don't have to set the mode to start the stream, but if you want to here is how:
    for (auto& mode : colorStream.available_modes())
    {
        if (mode.width() == 640 && mode.height() == 480)
        {
            colorStream.set_mode(mode);
            break;
        }
    }

    return colorStream;
}

int main(int argc, char** argv)
{
    astra::initialize();

    const char* licenseString = "<INSERT LICENSE KEY HERE>";
    orbbec_body_tracking_set_license(licenseString);

    set_key_handler();

    const char* windowTitle = "Masked Color Viewer";

    sf::RenderWindow window(sf::VideoMode(1280, 960), windowTitle);

#ifdef _WIN32
    auto fullscreenStyle = sf::Style::None;
#else
    auto fullscreenStyle = sf::Style::Fullscreen;
#endif

    const sf::VideoMode fullScreenMode = sf::VideoMode::getFullscreenModes()[0];
    const sf::VideoMode windowedMode(1280, 1024);
    bool isFullScreen = false;

    astra::StreamSet streamSet;
    astra::StreamReader reader = streamSet.create_reader();

    auto depthStream = configure_depth(reader);
    depthStream.start();

    reader.stream<astra::BodyStream>().start();

    auto colorStream = configure_color(reader);
    colorStream.start();

    reader.stream<astra::MaskedColorStream>().start();
    auto coordinateMapper = depthStream.coordinateMapper();
    MaskedColorFrameListener listener(coordinateMapper);

    const char* helpMessage =
        "keyboard shortcut:\n"
        "D      use 640x400 depth resolution\n"
        "F      toggle between fullscreen and windowed mode\n"
        "H      show/hide this message\n"
        "M      enable/disable depth mirroring\n"
        "N      enable/disable color mirroring\n"
        "P      enable/disable drawing texture\n"
        "SPACE  show/hide all text\n"
        "Esc    exit";
    listener.set_help_message(helpMessage);

    reader.add_listener(listener);

    while (window.isOpen())
    {
        astra_update();

        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::KeyPressed:
            {
                if (event.key.code == sf::Keyboard::C && event.key.control)
                {
                    window.close();
                }
                switch (event.key.code)
                {
                case sf::Keyboard::D:
                {
                    auto oldMode = depthStream.mode();
                    astra::ImageStreamMode depthMode;

                    depthMode.set_width(640);
                    depthMode.set_height(400);
                    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
                    depthMode.set_fps(30);

                    depthStream.set_mode(depthMode);
                    auto newMode = depthStream.mode();
                    printf("Changed depth mode: %dx%d @ %d -> %dx%d @ %d\n",
                           oldMode.width(), oldMode.height(), oldMode.fps(),
                           newMode.width(), newMode.height(), newMode.fps());
                    break;
                }
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::F:
                    if (isFullScreen)
                    {
                        window.create(windowedMode, windowTitle, sf::Style::Default);
                    }
                    else
                    {
                        window.create(fullScreenMode, windowTitle, fullscreenStyle);
                    }
                    isFullScreen = !isFullScreen;
                    break;
                case sf::Keyboard::H:
                    listener.toggle_help();
                    break;
                case sf::Keyboard::M:
                    depthStream.enable_mirroring(!depthStream.mirroring_enabled());
                    break;
                case sf::Keyboard::N:
                    colorStream.enable_mirroring(!colorStream.mirroring_enabled());
                    break;
                case sf::Keyboard::P:
                    listener.toggle_paused();
                    break;
                case sf::Keyboard::Space:
                    listener.toggle_overlay();
                    break;
                default:
                    break;
                }
                break;
            }
            case sf::Event::MouseMoved:
            {
                listener.update_mouse_position(window);
                break;
            }
            default:
                break;
            }
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        listener.draw_to(window);
        window.display();

        if (!shouldContinue)
        {
            window.close();
        }
    }

    astra::terminate();

    return 0;
}

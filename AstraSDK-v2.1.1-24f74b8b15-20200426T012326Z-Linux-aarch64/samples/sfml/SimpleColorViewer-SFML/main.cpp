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
#include <astra_core/astra_core.hpp>
#include <astra/astra.hpp>

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <key_handler.h>

class ColorFrameListener : public astra::FrameListener
{
public:
    ColorFrameListener()
    {
        prev_ = ClockType::now();
        font_.loadFromFile("Inconsolata.otf");
    }

    void init_texture(int width, int height)
    {
        if (displayBuffer_ == nullptr || width != displayWidth_ || height != displayHeight_)
        {
            displayWidth_ = width;
            displayHeight_ = height;

            // texture is RGBA
            int byteLength = displayWidth_ * displayHeight_ * 4;

            displayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::memset(displayBuffer_.get(), 0, byteLength);

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

    virtual void on_frame_ready(astra::StreamReader& reader, astra::Frame& frame) override
    {
        const astra::ColorFrame colorFrame = frame.get<astra::ColorFrame>();

        int width = colorFrame.width();
        int height = colorFrame.height();

        init_texture(width, height);

        check_fps();

        if (isPaused_)
        {
            return;
        }

        const astra::RgbPixel* colorData = colorFrame.data();

        for (int i = 0; i < width * height; i++)
        {
            int rgbaOffset = i * 4;
            displayBuffer_[rgbaOffset] = colorData[i].r;
            displayBuffer_[rgbaOffset + 1] = colorData[i].g;
            displayBuffer_[rgbaOffset + 2] = colorData[i].b;
            displayBuffer_[rgbaOffset + 3] = 255;
        }

        texture_.update(displayBuffer_.get());
    }

    void drawTo(sf::RenderWindow& window)
    {
        if (displayBuffer_ != nullptr)
        {
            float imageScale = window.getView().getSize().x / displayWidth_;
            sprite_.setScale(imageScale, imageScale);
            window.draw(sprite_);
            draw_help_message(window);
        }
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
    using DurationType = std::chrono::milliseconds;
    using ClockType = std::chrono::high_resolution_clock;

    ClockType::time_point prev_;
    float elapsedMillis_{.0f};

    sf::Texture texture_;
    sf::Sprite sprite_;
    sf::Font font_;

    using BufferPtr = std::unique_ptr<uint8_t[]>;
    BufferPtr displayBuffer_{nullptr};

    int displayWidth_{0};
    int displayHeight_{0};

    bool isPaused_{false};
    bool isMouseOverlayEnabled_{true};
    bool isFullHelpEnabled_{false};
    const char* helpMessage_{nullptr};
};

int main(int argc, char** argv)
{
    astra::initialize();

    set_key_handler();

    sf::RenderWindow window(sf::VideoMode(1280, 960), "Color Viewer");

    astra::StreamSet streamSet;
    astra::StreamReader reader = streamSet.create_reader();

    reader.stream<astra::ColorStream>().start();

    ColorFrameListener listener;

    const char* helpMessage =
        "keyboard shortcut:\n"
        "H      show/hide this message\n"
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
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::H:
                    listener.toggle_help();
                    break;
                case sf::Keyboard::P:
                    listener.toggle_paused();
                    break;
                case sf::Keyboard::Space:
                    listener.toggle_overlay();
                    break;
                }
                break;
            }
            default:
                break;
            }
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        listener.drawTo(window);
        window.display();

        if (!shouldContinue)
        {
            window.close();
        }
    }

    astra::terminate();
    return 0;
}

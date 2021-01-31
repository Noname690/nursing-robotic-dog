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
#include <iostream>
#include <cstring>
class sfLine : public sf::Drawable
{
public:
    sfLine(const sf::Vector2f& point1, const sf::Vector2f& point2, sf::Color color, float thickness)
        : color_(color)
    {
        const sf::Vector2f direction = point2 - point1;
        const sf::Vector2f unitDirection = direction / std::sqrt(direction.x*direction.x + direction.y*direction.y);
        const sf::Vector2f normal(-unitDirection.y, unitDirection.x);

        const sf::Vector2f offset = (thickness / 2.f) * normal;

        vertices_[0].position = point1 + offset;
        vertices_[1].position = point2 + offset;
        vertices_[2].position = point2 - offset;
        vertices_[3].position = point1 - offset;

        for (int i = 0; i<4; ++i)
            vertices_[i].color = color;
    }

    void draw(sf::RenderTarget &target, sf::RenderStates states) const
    {
        target.draw(vertices_, 4, sf::Quads, states);
    }

private:
    sf::Vertex vertices_[4];
    sf::Color color_;
};

class BodyVisualizer : public astra::FrameListener
{
public:
    static sf::Color get_body_color(std::uint8_t bodyId)
    {
        if (bodyId == 0)
        {
            // Handle no body separately - transparent
            return sf::Color(0x00, 0x00, 0x00, 0x00);
        }
        // Case 0 below could mean bodyId == 25 or
        // above due to the "% 24".
        switch (bodyId % 24) {
        case 0:
            return sf::Color(0x00, 0x88, 0x00, 0xFF);
        case 1:
            return sf::Color(0x00, 0x00, 0xFF, 0xFF);
        case 2:
            return sf::Color(0x88, 0x00, 0x00, 0xFF);
        case 3:
            return sf::Color(0x00, 0xFF, 0x00, 0xFF);
        case 4:
            return sf::Color(0x00, 0x00, 0x88, 0xFF);
        case 5:
            return sf::Color(0xFF, 0x00, 0x00, 0xFF);

        case 6:
            return sf::Color(0xFF, 0x88, 0x00, 0xFF);
        case 7:
            return sf::Color(0xFF, 0x00, 0xFF, 0xFF);
        case 8:
            return sf::Color(0x88, 0x00, 0xFF, 0xFF);
        case 9:
            return sf::Color(0x00, 0xFF, 0xFF, 0xFF);
        case 10:
            return sf::Color(0x00, 0xFF, 0x88, 0xFF);
        case 11:
            return sf::Color(0xFF, 0xFF, 0x00, 0xFF);

        case 12:
            return sf::Color(0x00, 0x88, 0x88, 0xFF);
        case 13:
            return sf::Color(0x00, 0x88, 0xFF, 0xFF);
        case 14:
            return sf::Color(0x88, 0x88, 0x00, 0xFF);
        case 15:
            return sf::Color(0x88, 0xFF, 0x00, 0xFF);
        case 16:
            return sf::Color(0x88, 0x00, 0x88, 0xFF);
        case 17:
            return sf::Color(0xFF, 0x00, 0x88, 0xFF);

        case 18:
            return sf::Color(0xFF, 0x88, 0x88, 0xFF);
        case 19:
            return sf::Color(0xFF, 0x88, 0xFF, 0xFF);
        case 20:
            return sf::Color(0x88, 0x88, 0xFF, 0xFF);
        case 21:
            return sf::Color(0x88, 0xFF, 0xFF, 0xFF);
        case 22:
            return sf::Color(0x88, 0xFF, 0x88, 0xFF);
        case 23:
            return sf::Color(0xFF, 0xFF, 0x88, 0xFF);
        default:
            return sf::Color(0xAA, 0xAA, 0xAA, 0xFF);
        }
    }

    void init_depth_texture(int width, int height)
    {
        if (displayBuffer_ == nullptr || width != depthWidth_ || height != depthHeight_)
        {
            depthWidth_ = width;
            depthHeight_ = height;
            int byteLength = depthWidth_ * depthHeight_ * 4;

            displayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::memset(displayBuffer_.get(), 0, byteLength);

            texture_.create(depthWidth_, depthHeight_);
            sprite_.setTexture(texture_, true);
            sprite_.setPosition(0, 0);
        }
    }

    void init_overlay_texture(int width, int height)
    {
        if (overlayBuffer_ == nullptr || width != overlayWidth_ || height != overlayHeight_)
        {
            overlayWidth_ = width;
            overlayHeight_ = height;
            int byteLength = overlayWidth_ * overlayHeight_ * 4;

            overlayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::fill(&overlayBuffer_[0], &overlayBuffer_[0] + byteLength, 0);

            overlayTexture_.create(overlayWidth_, overlayHeight_);
            overlaySprite_.setTexture(overlayTexture_, true);
            overlaySprite_.setPosition(0, 0);
        }
    }

    void check_fps()
    {
        double fpsFactor = 0.02;

        std::clock_t newTimepoint= std::clock();
        long double frameDuration = (newTimepoint - lastTimepoint_) / static_cast<long double>(CLOCKS_PER_SEC);

        frameDuration_ = frameDuration * fpsFactor + frameDuration_ * (1 - fpsFactor);
        lastTimepoint_ = newTimepoint;
        double fps = 1.0 / frameDuration_;

        printf("FPS: %3.1f (%3.4Lf ms)\n", fps, frameDuration_ * 1000);
    }

    void processDepth(astra::Frame& frame)
    {
        astra::DepthFrame depthFrame = frame.get<astra::DepthFrame>();

        if (!depthFrame.is_valid()) { return; }

        depthFrame.rotate(angle_);

        int width = depthFrame.width();
        int height = depthFrame.height();

        init_depth_texture(width, height);

        const int16_t* depthPtr = depthFrame.data();
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                int index = (x + y * width);
                int index4 = index * 4;

                int16_t depth = depthPtr[index];
                uint8_t value = depth % 255;

                displayBuffer_[index4] = value;
                displayBuffer_[index4 + 1] = value;
                displayBuffer_[index4 + 2] = value;
                displayBuffer_[index4 + 3] = 255;
            }
        }

        texture_.update(displayBuffer_.get());
    }

    void processBodies(astra::Frame& frame)
    {
        astra::BodyFrame bodyFrame = frame.get<astra::BodyFrame>();
        bodyFrame.rotate(angle_);

        jointPositions_.clear();
        circles_.clear();
        circleShadows_.clear();
        boneLines_.clear();
        boneShadows_.clear();

        if (!bodyFrame.is_valid() || bodyFrame.info().width() == 0 || bodyFrame.info().height() == 0)
        {
            clear_overlay();
            return;
        }

        const float jointScale = bodyFrame.info().width() / 120.f;

        const auto& bodies = bodyFrame.bodies();

        for (auto& body : bodies)
        {
            printf("Processing frame #%d body %d left hand: %u\n",
                bodyFrame.frame_index(), body.id(), unsigned(body.hand_poses().left_hand()));
            for(auto& joint : body.joints())
            {
                jointPositions_.push_back(joint.depth_position());
            }

            update_body(body, jointScale);
        }

        const auto& floor = bodyFrame.floor_info(); //floor
        if (floor.floor_detected())
        {
            const auto& p = floor.floor_plane();
            std::cout << "Floor plane: ["
                << p.a() << ", " << p.b() << ", " << p.c() << ", " << p.d()
                << "]" << std::endl;

        }

        const auto& bodyMask = bodyFrame.body_mask();
        const auto& floorMask = floor.floor_mask();

        update_overlay(bodyMask, floorMask);
    }

    void update_body(astra::Body body,
                     const float jointScale)
    {
        const auto& joints = body.joints();

        if (joints.empty())
        {
            return;
        }

        for (const auto& joint : joints)
        {
            astra::JointType type = joint.type();
            const auto& pos = joint.depth_position();

            if (joint.status() == astra::JointStatus::NotTracked)
            {
                continue;
            }

            auto radius = jointRadius_ * jointScale; // pixels
            sf::Color circleShadowColor(0, 0, 0, 255);

             auto color = sf::Color(0x00, 0xFF, 0x00, 0xFF);

            if ((type == astra::JointType::LeftHand && astra::HandPose::Grip==body.hand_poses().left_hand()) ||
                (type == astra::JointType::RightHand &&  astra::HandPose::Grip==body.hand_poses().right_hand()))
            {
                radius *= 1.5f;
                circleShadowColor = sf::Color(255, 255, 255, 255);
                color = sf::Color(0x00, 0xAA, 0xFF, 0xFF);
            }

            const auto shadowRadius = radius + shadowRadius_ * jointScale;
            const auto radiusDelta = shadowRadius - radius;

            sf::CircleShape circle(radius);

            circle.setFillColor(sf::Color(color.r, color.g, color.b, 255));
            circle.setPosition(pos.x - radius, pos.y - radius);
            circles_.push_back(circle);

            sf::CircleShape shadow(shadowRadius);
            shadow.setFillColor(circleShadowColor);
            shadow.setPosition(circle.getPosition() - sf::Vector2f(radiusDelta, radiusDelta));
            circleShadows_.push_back(shadow);
        }

        update_bone(joints, jointScale, astra::JointType::Head, astra::JointType::Neck);
        update_bone(joints, jointScale, astra::JointType::Neck, astra::JointType::ShoulderSpine);

        update_bone(joints, jointScale, astra::JointType::ShoulderSpine, astra::JointType::LeftShoulder);
        update_bone(joints, jointScale, astra::JointType::LeftShoulder, astra::JointType::LeftElbow);
        update_bone(joints, jointScale, astra::JointType::LeftElbow, astra::JointType::LeftWrist);
        update_bone(joints, jointScale, astra::JointType::LeftWrist, astra::JointType::LeftHand);

        update_bone(joints, jointScale, astra::JointType::ShoulderSpine, astra::JointType::RightShoulder);
        update_bone(joints, jointScale, astra::JointType::RightShoulder, astra::JointType::RightElbow);
        update_bone(joints, jointScale, astra::JointType::RightElbow, astra::JointType::RightWrist);
        update_bone(joints, jointScale, astra::JointType::RightWrist, astra::JointType::RightHand);

        update_bone(joints, jointScale, astra::JointType::ShoulderSpine, astra::JointType::MidSpine);
        update_bone(joints, jointScale, astra::JointType::MidSpine, astra::JointType::BaseSpine);

        update_bone(joints, jointScale, astra::JointType::BaseSpine, astra::JointType::LeftHip);
        update_bone(joints, jointScale, astra::JointType::LeftHip, astra::JointType::LeftKnee);
        update_bone(joints, jointScale, astra::JointType::LeftKnee, astra::JointType::LeftFoot);

        update_bone(joints, jointScale, astra::JointType::BaseSpine, astra::JointType::RightHip);
        update_bone(joints, jointScale, astra::JointType::RightHip, astra::JointType::RightKnee);
        update_bone(joints, jointScale, astra::JointType::RightKnee, astra::JointType::RightFoot);
    }

    void update_bone(const astra::JointList& joints,
                     const float jointScale,astra::JointType j1,
                     astra::JointType j2)
    {
        const auto& joint1 = joints[int(j1)];
        const auto& joint2 = joints[int(j2)];

        if (joint1.status() == astra::JointStatus::NotTracked ||
            joint2.status() == astra::JointStatus::NotTracked)
        {
            //don't render bones between untracked joints
            return;
        }

        //actually depth position, not world position
        const auto& jp1 = joint1.depth_position();
        const auto& jp2 = joint2.depth_position();

        auto p1 = sf::Vector2f(jp1.x, jp1.y);
        auto p2 = sf::Vector2f(jp2.x, jp2.y);

        sf::Color color(255, 255, 255, 255);
        float thickness = lineThickness_ * jointScale;
        if (joint1.status() == astra::JointStatus::LowConfidence ||
            joint2.status() == astra::JointStatus::LowConfidence)
        {
            color = sf::Color(128, 128, 128, 255);
            thickness *= 0.5f;
        }

        boneLines_.push_back(sfLine(p1,
            p2,
            color,
            thickness));
        const float shadowLineThickness = thickness + shadowRadius_ * jointScale * 2.f;
        boneShadows_.push_back(sfLine(p1,
            p2,
            sf::Color(0, 0, 0, 255),
            shadowLineThickness));
    }

    void update_overlay(const astra::BodyMask& bodyMask,
                        const astra::FloorMask& floorMask)
    {
        const auto* bodyData = bodyMask.data();
        const auto* floorData = floorMask.data();
        const int width = bodyMask.width();
        const int height = bodyMask.height();

        init_overlay_texture(width, height);

        const int length = width * height;

        for (int i = 0; i < length; i++)
        {
            const auto bodyId = bodyData[i];
            const auto isFloor = floorData[i];

            sf::Color color(0x0, 0x0, 0x0, 0x0);

            if (bodyId != 0)
            {
                color = get_body_color(bodyId);
            }
            else if (isFloor != 0)
            {
                color = sf::Color(0x0, 0x0, 0xFF, 0x88);
            }

            const int rgbaOffset = i * 4;
            overlayBuffer_[rgbaOffset] = color.r;
            overlayBuffer_[rgbaOffset + 1] = color.g;
            overlayBuffer_[rgbaOffset + 2] = color.b;
            overlayBuffer_[rgbaOffset + 3] = color.a;
        }

        overlayTexture_.update(overlayBuffer_.get());
    }

    void clear_overlay()
    {
        int byteLength = overlayWidth_ * overlayHeight_ * 4;
        std::fill(&overlayBuffer_[0], &overlayBuffer_[0] + byteLength, 0);

        overlayTexture_.update(overlayBuffer_.get());
    }

    virtual void on_frame_ready(astra::StreamReader& reader,
                                astra::Frame& frame) override
    {
        processDepth(frame);
        processBodies(frame);

        check_fps();
    }

    void draw_bodies(sf::RenderWindow& window)
    {
        const float scaleX = window.getView().getSize().x / overlayWidth_;
        const float scaleY = window.getView().getSize().y / overlayHeight_;

        sf::RenderStates states;
        sf::Transform transform;
        transform.scale(scaleX, scaleY);
        states.transform *= transform;

        for (const auto& bone : boneShadows_)
            window.draw(bone, states);

        for (const auto& c : circleShadows_)
            window.draw(c, states);

        for (const auto& bone : boneLines_)
            window.draw(bone, states);

        for (auto& c : circles_)
            window.draw(c, states);

    }

    void draw_to(sf::RenderWindow& window)
    {
        if (displayBuffer_ != nullptr)
        {
            const float scaleX = window.getView().getSize().x / depthWidth_;
            const float scaleY = window.getView().getSize().y / depthHeight_;
            sprite_.setScale(scaleX, scaleY);

            window.draw(sprite_); // depth
        }

        if (overlayBuffer_ != nullptr)
        {
            const float scaleX = window.getView().getSize().x / overlayWidth_;
            const float scaleY = window.getView().getSize().y / overlayHeight_;
            overlaySprite_.setScale(scaleX, scaleY);
            window.draw(overlaySprite_); //bodymask and floormask
        }

        draw_bodies(window);
    }

	void rotate(astra::BodyOrientation orientation)
	{
            if (orientation == astra::BodyOrientation::LEFT)
            {
                angle_ = 90;
            }
            else
            {
                angle_ = 270;
            }
	}

private:
    long double frameDuration_{ 0 };
    std::clock_t lastTimepoint_ { 0 };
    sf::Texture texture_;
    sf::Sprite sprite_;

    using BufferPtr = std::unique_ptr < uint8_t[] >;
    BufferPtr displayBuffer_{ nullptr };

    std::vector<astra::Vector2f> jointPositions_;

    int depthWidth_{0};
    int depthHeight_{0};
    int overlayWidth_{0};
    int overlayHeight_{0};

    std::vector<sfLine> boneLines_;
    std::vector<sfLine> boneShadows_;
    std::vector<sf::CircleShape> circles_;
    std::vector<sf::CircleShape> circleShadows_;

    float lineThickness_{ 0.5f }; // pixels
    float jointRadius_{ 1.0f };   // pixels
    float shadowRadius_{ 0.5f };  // pixels

    BufferPtr overlayBuffer_{ nullptr };
    sf::Texture overlayTexture_;
    sf::Sprite overlaySprite_;
    int angle_ = 0;

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

char licenseString[4096];
int main(int argc, char** argv)
{
    astra::initialize();

    const char* licenseString = "<INSERT LICENSE KEY HERE>";
    orbbec_body_tracking_set_license(licenseString);

    sf::RenderWindow window(sf::VideoMode(480, 640), "Rotated Body Viewer");

#ifdef _WIN32
    auto fullscreenStyle = sf::Style::None;
#else
    auto fullscreenStyle = sf::Style::Fullscreen;
#endif

    const sf::VideoMode fullScreenMode = sf::VideoMode::getFullscreenModes()[0];
    const sf::VideoMode windowedMode(480, 640);
    bool isFullScreen = false;

    astra::StreamSet sensor;
    astra::StreamReader reader = sensor.create_reader();

    BodyVisualizer listener;

    auto depthStream = configure_depth(reader);
    depthStream.start();

    auto bodyStream = reader.stream<astra::BodyStream>();

    astra::BodyOrientation orientation = astra::BodyOrientation::LEFT;
    bodyStream.set_body_orientation(orientation);
    listener.rotate(orientation);

    bodyStream.start();

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
                case sf::Keyboard::F:
                    if (isFullScreen)
                    {
                        window.create(windowedMode, "Rotated Body Viewer", sf::Style::Default);
                    }
                    else
                    {
                        window.create(fullScreenMode, "Rotated Body Viewer", fullscreenStyle);
                    }
                    isFullScreen = !isFullScreen;
                    break;
                case sf::Keyboard::O:
                    if (orientation == astra::BodyOrientation::LEFT)
                    {
                        orientation = astra::BodyOrientation::RIGHT;
                    }
                    else
                    {
                        orientation = astra::BodyOrientation::LEFT;
                    }
                    bodyStream.set_body_orientation(orientation);
                    listener.rotate(orientation);
                    break;
                case sf::Keyboard::M:
                    depthStream.enable_mirroring(!depthStream.mirroring_enabled());
                    break;
                default:
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

        listener.draw_to(window);
        window.display();
    }

    astra::terminate();

    return 0;
}

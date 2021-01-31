#include <SFML/Graphics.hpp>
#include <astra/astra.hpp>
#include <iostream>
#include <cstring>
#include <sstream>
#include <cmath>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <stdlib.h>
#include <string>
using namespace std;
string posture = "";
float waist[3][3];
float manDis = 0;
float angle = 0;
class sfLine : public sf::Drawable
{
public:
    sfLine(const sf::Vector2f& point1, const sf::Vector2f& point2, sf::Color color, float thickness)
        : color_(color)
    {
        const sf::Vector2f direction = point2 - point1;
        const sf::Vector2f unitDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);
        const sf::Vector2f normal(-unitDirection.y, unitDirection.x);

        const sf::Vector2f offset = (thickness / 2.f) * normal;

        vertices_[0].position = point1 + offset;
        vertices_[1].position = point2 + offset;
        vertices_[2].position = point2 - offset;
        vertices_[3].position = point1 - offset;

        for (int i = 0; i < 4; ++i)
            vertices_[i].color = color;
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const
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
    BodyVisualizer()
    {
        font_.loadFromFile("Inconsolata.otf");
    }

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

        std::clock_t newTimepoint = std::clock();
        long double frameDuration = (newTimepoint - lastTimepoint_) / static_cast<long double>(CLOCKS_PER_SEC);

        frameDuration_ = frameDuration * fpsFactor + frameDuration_ * (1 - fpsFactor);
        lastTimepoint_ = newTimepoint;
        double fps = 1.0 / frameDuration_;

        printf("FPS: %3.1f (%3.4Lf ms)\n", fps, frameDuration_ * 1000);
        posture+="FPS:";
        stringstream out1;
        out1<<fixed<<setprecision(3)<<fps;
        string s10 = out1.str();
        posture+=s10;
        posture+="\n";
    }

    void processDepth(astra::Frame& frame)
    {
        const astra::DepthFrame depthFrame = frame.get<astra::DepthFrame>();

        if (!depthFrame.is_valid()) { return; }

        int width = depthFrame.width();
        int height = depthFrame.height();

        init_depth_texture(width, height);

        const int16_t* depthPtr = depthFrame.data();
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
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
            // printf("Processing%d frame #%d body %d left hand: %u\n",
            //   bodyFrame.frame_index(), body.id(), unsigned(body.hand_poses().left_hand()),body.id());
            printf("frame_index:%d \nbody_id:%d \ncenter_of_mass:%f %f %f  \n",
                bodyFrame.frame_index(), body.id(), body.center_of_mass().x, body.center_of_mass().y, body.center_of_mass().z);
            for (auto& joint : body.joints())
            {
                printf("joint_info:\ntype:%d\nworld_position:%f %f %f\ndepth_position:%f %f\n",
                    joint.type(), joint.world_position().x, joint.world_position().y, joint.world_position().z, joint.depth_position().x, joint.depth_position().y);
                jointPositions_.push_back(joint.depth_position());
            }
        posture += "unknown";
        
        manDis = sqrt(body.joints()[9].world_position().x * body.joints()[9].world_position().x + body.joints()[9].world_position().z * body.joints()[9].world_position().z);
        
        if (body.center_of_mass().y - body.joints()[12].world_position().y < 400) {
            posture+="accident";
        }else{
            posture+="safe";
        }
        posture+="\ndistance:";
        manDis = manDis/1000.0;
        stringstream out;
        out<<fixed<<setprecision(3)<<manDis;
        string s5 = out.str();
        posture+=s5;

        posture+="m\nangle:";

                
        stringstream out2;
        angle = atan2(body.joints()[9].world_position().z, body.joints()[9].world_position().x);
        angle = body.joints()[9].world_position().x;
        out2<<fixed<<setprecision(3)<<angle;
        s5 = out2.str();
        posture+=s5;
            for (size_t i = 2; i > 0; i--)
            {
                for (size_t j = 0; j < 3; j++) {
                    waist[i][j] = waist[i - 1][j];
                }
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

            if ((type == astra::JointType::LeftHand && astra::HandPose::Grip == body.hand_poses().left_hand()) ||
                (type == astra::JointType::RightHand && astra::HandPose::Grip == body.hand_poses().right_hand()))
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
        const float jointScale, astra::JointType j1,
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

        check_fps();
        if (isPaused_) { return; }

        processDepth(frame);
        processBodies(frame);
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
        str << posture;

        if (isFullHelpEnabled_ && helpMessage_ != nullptr)
        {
            str << "\n" << helpMessage_;
        }

        const int characterSize = 150;
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
        draw_help_message(window);
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
    long double frameDuration_{ 0 };
    std::clock_t lastTimepoint_{ 0 };
    sf::Texture texture_;
    sf::Sprite sprite_;
    sf::Font font_;

    using BufferPtr = std::unique_ptr < uint8_t[] >;
    BufferPtr displayBuffer_{ nullptr };

    std::vector<astra::Vector2f> jointPositions_;

    int depthWidth_{ 0 };
    int depthHeight_{ 0 };
    int overlayWidth_{ 0 };
    int overlayHeight_{ 0 };

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

    bool isPaused_{ false };
    bool isMouseOverlayEnabled_{ true };
    bool isFullHelpEnabled_{ false };
    const char* helpMessage_{ nullptr };
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
void robotMove(double lx,double ly,double lz,double ax,double ay,double az,ros::Publisher &pub,ros::Rate &rate){
    if(ros::ok){
        geometry_msgs::Twist msg;
        msg.linear.x = lx;
        msg.linear.y = ly;
        msg.linear.z = lz;
        msg.angular.x = ax;
        msg.angular.y = ay;
        msg.angular.z = az;
        pub.publish(msg);
        ROS_INFO_STREAM("Sending random velocity command:"<<" linear="<<msg.linear.x<<" angular="<<msg.angular.z);
        rate.sleep();
    }
}

int main(int argc, char** argv)
{
    for (size_t i = 0; i < 3; i++)
    {
        for (size_t j = 0; j < 3; j++) {
            waist[i][j] = 0;
        }
    }
    astra::initialize();

    if (argc == 2)
    {
        FILE* fp = fopen(argv[1], "rb");
        char licenseString[1024] = { 0 };
        fread(licenseString, 1, 1024, fp);
        orbbec_body_tracking_set_license(licenseString);

        fclose(fp);
    }
    else
    {
        const char* licenseString = "<INSERT LICENSE KEY HERE>";
        orbbec_body_tracking_set_license(licenseString);
    }

    sf::RenderWindow window(sf::VideoMode(1280, 960), "Simple Body Viewer");

#ifdef _WIN32
    auto fullscreenStyle = sf::Style::None;
#else
    auto fullscreenStyle = sf::Style::Fullscreen;
#endif

    const sf::VideoMode fullScreenMode = sf::VideoMode::getFullscreenModes()[0];
    const sf::VideoMode windowedMode(1280, 960);
    bool isFullScreen = false;

    astra::StreamSet sensor;
    astra::StreamReader reader = sensor.create_reader();

    BodyVisualizer listener;

    auto depthStream = configure_depth(reader);
    depthStream.start();

    auto bodyStream = reader.stream<astra::BodyStream>();
    bodyStream.start();
    reader.add_listener(listener);

    //astra::SkeletonProfile profile = bodyStream.get_skeleton_profile();
    astra::SkeletonProfile profile = astra::SkeletonProfile::Full;
    astra::BodyTrackingFeatureFlags features = astra::BodyTrackingFeatureFlags::HandPoses;

    ros::init(argc, argv, "publish_velocity");
    ros::NodeHandle nh;
    ros::Publisher pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 1000);
    srand(time(0));
    ros::Rate rate(2);

    while (window.isOpen())
    {

        astra_update();
        sf::Event event;
    double move = 0;
    double zhuan = 0.0;
    if(manDis > 2.5) move = 1.0;
    if(angle>50)	zhuan = 0.3;
    else if(angle<-50)	zhuan = -0.3;
    robotMove(move,0,0,0,0,zhuan,pub,rate);

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                window.close();
                break;
            }
            bodyStream.set_skeleton_profile(profile);
            bodyStream.set_default_body_features(features);

            //listener.processBodies(reader.get_latest_frame());
        }
        window.clear(sf::Color::Black);
        listener.draw_to(window);
        window.display();
    }
    astra::terminate();
    return 0;
}

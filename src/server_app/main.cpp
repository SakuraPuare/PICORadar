#include <iostream>
#include "player_data.pb.h" // 包含生成的Protobuf头文件

int main(int argc, char* argv[]) {
    // 验证Protobuf代码生成是否正常工作
    picoradar::PlayerData player;
    player.set_player_id("player_01");
    player.mutable_position()->set_x(1.0f);
    player.mutable_position()->set_y(2.0f);
    player.mutable_position()->set_z(3.0f);

    std::cout << "PICO Radar Server" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "Successfully initialized." << std::endl;
    std::cout << "Protobuf test: Player ID '" << player.player_id() 
              << "' at position (" << player.position().x()
              << ", " << player.position().y()
              << ", " << player.position().z() << ")" << std::endl;

    // 检查命令行参数
    if (argc > 1) {
        std::cout << "Launched with " << argc - 1 << " arguments:" << std::endl;
        for (int i = 1; i < argc; ++i) {
            std::cout << "  - " << argv[i] << std::endl;
        }
    }

    // TODO:
    // 1. 初始化WebSocket服务器
    // 2. 启动监听循环
    // 3. 实现服务发现
    // 4. ...

    return 0;
}

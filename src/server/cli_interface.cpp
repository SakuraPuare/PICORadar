#include "cli_interface.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using namespace ftxui;

namespace picoradar::server {

CLIInterface::CLIInterface() = default;

CLIInterface::~CLIInterface() {
    stop();
}

void CLIInterface::start() {
    if (running_.exchange(true)) {
        return; // 已经在运行
    }
    
    ui_thread_ = std::make_unique<std::thread>([this]() {
        auto screen = ScreenInteractive::TerminalOutput();
        
        auto ui = createUI();
        
        // 定期刷新界面
        auto refresh_timer = std::thread([this, &screen]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (needs_refresh_.exchange(false)) {
                    screen.PostEvent(Event::Custom);
                }
            }
        });
        
        screen.Loop(ui);
        
        refresh_timer.join();
    });
}

void CLIInterface::stop() {
    if (running_.exchange(false)) {
        if (ui_thread_ && ui_thread_->joinable()) {
            ui_thread_->join();
        }
    }
}

void CLIInterface::addLogEntry(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // 生成时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    log_entries_.push_back({oss.str(), level, message});
    
    // 限制日志条目数量
    if (log_entries_.size() > MAX_LOG_ENTRIES) {
        log_entries_.pop_front();
    }
    
    needs_refresh_ = true;
}

void CLIInterface::updateServerStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    server_status_ = status;
    needs_refresh_ = true;
}

void CLIInterface::updateConnectionCount(int count) {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    connection_count_ = count;
    needs_refresh_ = true;
}

void CLIInterface::updateMessageStats(int received, int sent) {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    messages_received_ = received;
    messages_sent_ = sent;
    needs_refresh_ = true;
}

void CLIInterface::setCommandHandler(std::function<void(const std::string&)> handler) {
    command_handler_ = std::move(handler);
}

Component CLIInterface::createUI() {
    // 命令输入组件
    auto input_component = Input(&command_input_, "输入命令...");
    
    // 处理回车键
    auto command_processor = CatchEvent(input_component, [this](Event event) {
        if (event == Event::Return && !command_input_.empty()) {
            if (command_handler_) {
                command_handler_(command_input_);
            }
            command_input_.clear();
            return true;
        }
        return false;
    });
    
    // 主界面组件
    auto main_component = Renderer(command_processor, [this, command_processor]() {
        Elements title_elements = {
            text("🎯 PICO Radar Server") | bold | color(Color::Green),
            filler(),
            text("Ctrl+C 退出") | color(Color::Yellow)
        };
        
        Elements left_panel_elements = {
            renderStatus(),
            separator(),
            renderStats()
        };
        
        Elements main_content_elements = {
            vbox(left_panel_elements) | size(WIDTH, EQUAL, 30),
            separator(),
            renderLogs() | flex
        };
        
        Elements command_elements = {
            text("命令: ") | color(Color::Blue),
            command_processor->Render() | flex
        };
        
        Elements main_elements = {
            hbox(title_elements) | border,
            hbox(main_content_elements) | flex,
            separator(),
            hbox(command_elements) | border
        };
        
        return vbox(main_elements);
    });
    
    return main_component;
}

Element CLIInterface::renderStatus() {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    
    Color status_color = Color::Green;
    if (server_status_.find("错误") != std::string::npos || 
        server_status_.find("停止") != std::string::npos) {
        status_color = Color::Red;
    } else if (server_status_.find("警告") != std::string::npos) {
        status_color = Color::Yellow;
    }
    
    Elements status_elements = {
        text("📊 服务器状态") | bold | color(Color::Cyan),
        separator(),
        hbox(Elements{text("状态: "), text(server_status_) | color(status_color)}),
        hbox(Elements{text("连接数: "), text(std::to_string(connection_count_)) | color(Color::Green)})
    };
    
    return vbox(status_elements) | border;
}

Element CLIInterface::renderLogs() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    Elements log_elements;
    
    // 标题
    log_elements.push_back(
        text("📋 实时日志") | bold | color(Color::Cyan)
    );
    log_elements.push_back(separator());
    
    // 日志条目（显示最新的）
    size_t start = log_entries_.size() > 50 ? log_entries_.size() - 50 : 0;
    for (size_t i = start; i < log_entries_.size(); ++i) {
        const auto& entry = log_entries_[i];
        
        Color level_color = Color::White;
        if (entry.level == "ERROR" || entry.level == "FATAL") {
            level_color = Color::Red;
        } else if (entry.level == "WARNING") {
            level_color = Color::Yellow;
        } else if (entry.level == "INFO") {
            level_color = Color::Green;
        } else if (entry.level == "DEBUG") {
            level_color = Color::Blue;
        }
        
        Elements log_line_elements = {
            text(entry.timestamp) | color(Color::GrayDark),
            text(" ["),
            text(entry.level) | color(level_color),
            text("] "),
            text(entry.message)
        };
        
        log_elements.push_back(hbox(log_line_elements));
    }
    
    if (log_elements.size() == 2) { // 只有标题和分隔符
        log_elements.push_back(text("暂无日志") | color(Color::GrayDark));
    }
    
    return vbox(log_elements) | border | vscroll_indicator | yframe;
}

Element CLIInterface::renderStats() {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    
    Elements stats_elements = {
        text("📈 消息统计") | bold | color(Color::Cyan),
        separator(),
        hbox(Elements{text("接收: "), text(std::to_string(messages_received_)) | color(Color::Blue)}),
        hbox(Elements{text("发送: "), text(std::to_string(messages_sent_)) | color(Color::Blue)}),
        separator(),
        text("🔧 可用命令") | bold | color(Color::Magenta),
        text("• status - 显示详细状态"),
        text("• connections - 列出连接"),
        text("• restart - 重启服务"),
        text("• help - 显示帮助")
    };
    
    return vbox(stats_elements) | border;
}

} // namespace picoradar::server

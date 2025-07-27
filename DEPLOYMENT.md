# PICORadar 部署指南

本文档提供 PICORadar 系统在生产环境中的部署指南，包括服务器配置、网络设置、监控和维护。

## 目录

- [系统架构](#系统架构)
- [硬件要求](#硬件要求)
- [网络配置](#网络配置)
- [服务器部署](#服务器部署)
- [监控和维护](#监控和维护)
- [故障排除](#故障排除)

## 系统架构

### 典型部署架构

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   VR设备 #1     │    │   VR设备 #2     │    │   VR设备 #N     │
│  (客户端库)      │    │  (客户端库)      │    │  (客户端库)      │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                    ┌─────────────┴─────────────┐
                    │      局域网交换机         │
                    │    (Gigabit Ethernet)    │
                    └─────────────┬─────────────┘
                                 │
                    ┌─────────────┴─────────────┐
                    │   PICORadar 服务器       │
                    │   - WebSocket Server      │
                    │   - UDP Discovery         │
                    │   - 监控界面              │
                    └───────────────────────────┘
```

### 网络端口

- **9002/TCP**: WebSocket 主服务端口
- **9001/UDP**: 服务发现广播端口
- **22/TCP**: SSH 管理端口（可选）
- **80/443**: Web 监控界面（未来功能）

## 硬件要求

### 最低配置

- **CPU**: 双核 2.0GHz 或更高
- **内存**: 4GB RAM
- **存储**: 20GB 可用空间
- **网络**: Gigabit Ethernet 网卡

### 推荐配置

- **CPU**: 四核 3.0GHz 或更高（Intel i5/AMD Ryzen 5 级别）
- **内存**: 8GB RAM 或更高
- **存储**: 50GB SSD
- **网络**: Gigabit Ethernet 网卡，低延迟交换机

### 高负载配置（20+ 并发用户）

- **CPU**: 八核 3.5GHz 或更高（Intel i7/AMD Ryzen 7 级别）
- **内存**: 16GB RAM 或更高
- **存储**: 100GB NVMe SSD
- **网络**: 10Gbps 网卡（如果可用）

## 网络配置

### 网络要求

#### 带宽要求

每个客户端的带宽使用估算：

- **上行**: 约 1-2 Kbps（发送自己的位置数据）
- **下行**: 约 20-40 Kbps（接收所有其他玩家数据，假设 20 人）
- **总计**: 每客户端约 50 Kbps

20 个客户端的总带宽需求：约 1 Mbps

#### 延迟要求

- **局域网延迟**: < 5ms（推荐）
- **端到端延迟**: < 100ms（系统要求）
- **抖动**: < 10ms

### 防火墙配置

#### Linux (iptables)

```bash
# 允许 SSH 连接
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 允许 WebSocket 连接
sudo iptables -A INPUT -p tcp --dport 9002 -j ACCEPT

# 允许 UDP 服务发现
sudo iptables -A INPUT -p udp --dport 9001 -j ACCEPT
sudo iptables -A OUTPUT -p udp --dport 9001 -j ACCEPT

# 允许相关和已建立的连接
sudo iptables -A INPUT -m state --state RELATED,ESTABLISHED -j ACCEPT

# 保存规则
sudo iptables-save > /etc/iptables/rules.v4
```

#### Windows Defender 防火墙

```powershell
# 允许入站连接
New-NetFirewallRule -DisplayName "PICORadar WebSocket" -Direction Inbound -Protocol TCP -LocalPort 9002 -Action Allow
New-NetFirewallRule -DisplayName "PICORadar Discovery" -Direction Inbound -Protocol UDP -LocalPort 9001 -Action Allow

# 允许出站连接
New-NetFirewallRule -DisplayName "PICORadar Discovery Out" -Direction Outbound -Protocol UDP -LocalPort 9001 -Action Allow
```

### 路由器配置

如果需要跨网段访问，配置静态路由：

```bash
# 添加静态路由（示例）
sudo route add -net 192.168.2.0/24 gw 192.168.1.1
```

## 服务器部署

### 1. 环境准备

#### 创建专用用户

```bash
# 创建 picoradar 用户
sudo useradd -r -s /bin/bash -d /opt/picoradar picoradar
sudo mkdir -p /opt/picoradar
sudo chown picoradar:picoradar /opt/picoradar
```

#### 安装运行时依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y libboost-system-dev libprotobuf23 libgoogle-glog0v5

# CentOS/RHEL
sudo yum install -y boost-system protobuf glog
```

### 2. 部署二进制文件

```bash
# 切换到 picoradar 用户
sudo su - picoradar

# 创建目录结构
mkdir -p /opt/picoradar/{bin,config,logs,data}

# 复制编译好的二进制文件
cp build/src/server/server /opt/picoradar/bin/
cp build/examples/* /opt/picoradar/bin/

# 设置执行权限
chmod +x /opt/picoradar/bin/*
```

### 3. 配置文件

创建生产环境配置 `/opt/picoradar/config/server.json`：

```json
{
  "server": {
    "port": 9002,
    "host": "0.0.0.0",
    "max_connections": 20,
    "heartbeat_interval_ms": 30000,
    "connection_timeout_ms": 60000
  },
  "discovery": {
    "enabled": true,
    "port": 9001,
    "broadcast_interval_ms": 2000,
    "server_name": "PICORadar-Production"
  },
  "authentication": {
    "enabled": true,
    "token": "${PICORADAR_AUTH_TOKEN}",
    "token_validation_timeout_ms": 5000
  },
  "logging": {
    "level": "INFO",
    "file_logging": true,
    "console_logging": false,
    "max_log_file_size_mb": 100,
    "max_log_files": 10,
    "log_directory": "/opt/picoradar/logs"
  },
  "performance": {
    "thread_pool_size": 4,
    "message_queue_size": 1000,
    "compression_enabled": false
  }
}
```

### 4. 环境变量配置

创建 `/opt/picoradar/.env` 文件：

```bash
# 服务器配置
export PICORADAR_CONFIG_FILE="/opt/picoradar/config/server.json"
export PICORADAR_AUTH_TOKEN="your-production-secret-token-here"
export PICORADAR_LOG_LEVEL="INFO"

# 性能调优
export PICORADAR_THREAD_POOL_SIZE=4
export PICORADAR_MAX_CONNECTIONS=20

# 系统配置
export LD_LIBRARY_PATH="/opt/picoradar/lib:$LD_LIBRARY_PATH"
```

### 5. 系统服务配置

#### 创建 systemd 服务文件

`/etc/systemd/system/picoradar.service`：

```ini
[Unit]
Description=PICORadar Server
After=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=5
User=picoradar
Group=picoradar
WorkingDirectory=/opt/picoradar
EnvironmentFile=/opt/picoradar/.env
ExecStart=/opt/picoradar/bin/server --config /opt/picoradar/config/server.json
StandardOutput=journal
StandardError=journal
SyslogIdentifier=picoradar

# 安全设置
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/picoradar

# 资源限制
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
```

#### 启动服务

```bash
# 重新加载 systemd 配置
sudo systemctl daemon-reload

# 启用自动启动
sudo systemctl enable picoradar

# 启动服务
sudo systemctl start picoradar

# 检查状态
sudo systemctl status picoradar
```

### 6. 日志轮转配置

创建 `/etc/logrotate.d/picoradar`：

```
/opt/picoradar/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 0644 picoradar picoradar
    postrotate
        systemctl reload picoradar
    endscript
}
```

## 监控和维护

### 1. 系统监控

#### 基本健康检查脚本

创建 `/opt/picoradar/scripts/health_check.sh`：

```bash
#!/bin/bash

SERVICE_NAME="picoradar"
LOG_FILE="/opt/picoradar/logs/health_check.log"
DATE=$(date '+%Y-%m-%d %H:%M:%S')

# 检查服务状态
if ! systemctl is-active --quiet $SERVICE_NAME; then
    echo "[$DATE] ERROR: $SERVICE_NAME service is not running" >> $LOG_FILE
    systemctl restart $SERVICE_NAME
    exit 1
fi

# 检查端口监听
if ! netstat -ln | grep -q ":9002 "; then
    echo "[$DATE] ERROR: Port 9002 is not listening" >> $LOG_FILE
    systemctl restart $SERVICE_NAME
    exit 1
fi

# 检查内存使用
MEMORY_USAGE=$(ps aux | grep "server" | grep -v grep | awk '{print $4}' | head -1)
if [ $(echo "$MEMORY_USAGE > 80" | bc -l) -eq 1 ]; then
    echo "[$DATE] WARNING: High memory usage: $MEMORY_USAGE%" >> $LOG_FILE
fi

echo "[$DATE] INFO: Health check passed" >> $LOG_FILE
exit 0
```

#### 设置定期健康检查

```bash
# 添加到 crontab
crontab -e

# 每 5 分钟检查一次
*/5 * * * * /opt/picoradar/scripts/health_check.sh
```

### 2. 性能监控

#### 资源使用监控脚本

创建 `/opt/picoradar/scripts/monitor.sh`：

```bash
#!/bin/bash

LOG_FILE="/opt/picoradar/logs/performance.log"
DATE=$(date '+%Y-%m-%d %H:%M:%S')

# 获取进程 PID
PID=$(pgrep -f "picoradar.*server")

if [ -z "$PID" ]; then
    echo "[$DATE] ERROR: PICORadar server process not found" >> $LOG_FILE
    exit 1
fi

# CPU 使用率
CPU_USAGE=$(ps -p $PID -o %cpu --no-headers)

# 内存使用率
MEMORY_USAGE=$(ps -p $PID -o %mem --no-headers)

# 文件描述符使用
FD_COUNT=$(lsof -p $PID | wc -l)

# 网络连接数
CONN_COUNT=$(netstat -np | grep $PID | wc -l)

echo "[$DATE] CPU: $CPU_USAGE%, Memory: $MEMORY_USAGE%, FD: $FD_COUNT, Connections: $CONN_COUNT" >> $LOG_FILE
```

### 3. 日志分析

#### 日志聚合脚本

创建 `/opt/picoradar/scripts/log_analysis.sh`：

```bash
#!/bin/bash

LOG_DIR="/opt/picoradar/logs"
REPORT_FILE="/opt/picoradar/logs/daily_report.txt"
DATE=$(date '+%Y-%m-%d')

echo "PICORadar Daily Report - $DATE" > $REPORT_FILE
echo "=================================" >> $REPORT_FILE

# 错误统计
echo "" >> $REPORT_FILE
echo "Error Summary:" >> $REPORT_FILE
grep "ERROR" $LOG_DIR/server.ERROR* | wc -l | xargs echo "Total Errors:" >> $REPORT_FILE

# 连接统计
echo "" >> $REPORT_FILE
echo "Connection Summary:" >> $REPORT_FILE
grep "Client connected" $LOG_DIR/server.INFO* | wc -l | xargs echo "New Connections:" >> $REPORT_FILE
grep "Client disconnected" $LOG_DIR/server.INFO* | wc -l | xargs echo "Disconnections:" >> $REPORT_FILE

# 性能统计
echo "" >> $REPORT_FILE
echo "Performance Summary:" >> $REPORT_FILE
tail -100 $LOG_DIR/performance.log | awk -F'CPU: |%, Memory: |%, FD: |, Connections:' '{sum_cpu+=$2; sum_mem+=$3; count++} END {print "Average CPU: " sum_cpu/count "%, Average Memory: " sum_mem/count "%"}' >> $REPORT_FILE
```

### 4. 备份策略

#### 配置文件备份

```bash
#!/bin/bash
# backup_config.sh

BACKUP_DIR="/opt/picoradar/backups"
DATE=$(date '+%Y%m%d_%H%M%S')

mkdir -p $BACKUP_DIR

# 备份配置文件
tar -czf $BACKUP_DIR/config_$DATE.tar.gz /opt/picoradar/config/

# 备份日志文件（最近7天）
find /opt/picoradar/logs -name "*.log" -mtime -7 -exec tar -czf $BACKUP_DIR/logs_$DATE.tar.gz {} +

# 清理老的备份文件（保留30天）
find $BACKUP_DIR -name "*.tar.gz" -mtime +30 -delete
```

## 故障排除

### 常见问题

#### 1. 服务无法启动

**症状**: `systemctl start picoradar` 失败

**诊断步骤**:

```bash
# 查看服务状态
systemctl status picoradar

# 查看详细日志
journalctl -u picoradar -f

# 检查配置文件语法
/opt/picoradar/bin/server --config /opt/picoradar/config/server.json --validate-config
```

**常见原因**:
- 配置文件语法错误
- 端口被占用
- 权限不足
- 依赖库缺失

#### 2. 客户端无法连接

**症状**: 客户端连接超时或被拒绝

**诊断步骤**:

```bash
# 检查端口监听
netstat -tlnp | grep 9002

# 检查防火墙
iptables -L -n | grep 9002

# 测试连接性
telnet <server_ip> 9002

# 检查服务发现
tcpdump -i any -n port 9001
```

#### 3. 性能问题

**症状**: 延迟高或连接不稳定

**诊断步骤**:

```bash
# 检查系统负载
top
htop

# 检查网络质量
ping <client_ip>
iperf3 -s  # 在服务器上
iperf3 -c <server_ip>  # 在客户端上

# 检查文件描述符限制
ulimit -n
lsof -p $(pgrep server) | wc -l
```

#### 4. 内存泄漏

**症状**: 内存使用持续增长

**诊断步骤**:

```bash
# 监控内存使用
watch -n 5 'ps aux | grep server'

# 使用 Valgrind 检查（开发环境）
valgrind --tool=memcheck --leak-check=full ./server

# 生成内存报告
pmap -x $(pgrep server)
```

### 紧急恢复程序

#### 快速重启

```bash
#!/bin/bash
# emergency_restart.sh

echo "Emergency restart initiated at $(date)"

# 停止服务
systemctl stop picoradar

# 备份当前日志
cp /opt/picoradar/logs/server.log /opt/picoradar/logs/server.log.$(date +%s)

# 清理临时文件
rm -f /tmp/picoradar_*

# 重启服务
systemctl start picoradar

# 验证服务状态
sleep 5
if systemctl is-active --quiet picoradar; then
    echo "Service restarted successfully"
else
    echo "Service restart failed"
    systemctl status picoradar
fi
```

### 联系支持

如果问题无法解决，请收集以下信息并联系技术支持：

1. **系统信息**: `uname -a`, `cat /etc/os-release`
2. **服务状态**: `systemctl status picoradar`
3. **配置文件**: `/opt/picoradar/config/server.json`
4. **最新日志**: 最近1小时的服务器日志
5. **网络配置**: `ip addr show`, `route -n`
6. **资源使用**: `top`, `free -h`, `df -h`

---

更多信息请参考：
- [安装指南](INSTALLATION.md)
- [API 参考文档](docs/API_REFERENCE.md)
- [技术设计文档](TECHNICAL_DESIGN.md)

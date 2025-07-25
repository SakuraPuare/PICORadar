#include <glog/logging.h>

#include <iostream>
#include <string>
#include <vector>

#include "common/constants.hpp"
#include "common/logging.hpp"
#include "sync_client.hpp"

auto main(int argc, char* argv[]) -> int {
  // 初始化 glog
  google::InitGoogleLogging(argv[0]);
  picoradar::common::setup_logging(argv[0], false);

  if (argc < 2) {
    LOG(ERROR) << "Usage: " << argv[0] << " <host> <port> <mode> [player_id]";
    LOG(ERROR) << "Or:    " << argv[0] << " --discover [player_id]";
    return 1;
  }

  picoradar::mock_client::SyncClient client;

  std::string arg1 = argv[1];

  if (arg1 == "--discover") {
    std::string player_id = (argc > 2) ? argv[2] : "discovery_tester";
    return client.discover_and_run(player_id,
                                   picoradar::config::kDefaultDiscoveryPort);
  }

  if (argc < 4) {
    LOG(ERROR) << "Usage: " << argv[0] << " <host> <port> <mode> [player_id]";
    return 1;
  }

  std::string host = argv[1];
  std::string port = argv[2];
  std::string mode = argv[3];
  std::string player_id = (argc > 4) ? argv[4] : "default_player";

  client.run(host, port, mode, player_id);

  return 0;
}
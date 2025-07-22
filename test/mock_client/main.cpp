#include <glog/logging.h>
#include "mock_client/sync_client.hpp"
#include "common/logging.hpp"

auto main(int argc, char** argv) -> int {
  picoradar::common::setup_logging(argv[0], false);

  if (argc < 2) {
    LOG(ERROR) << "Usage: \n"
               << "  " << argv[0]
               << " <host> <port> <player_id> [mode]\n"
               << "  " << argv[0] << " --discover <player_id>";
    return 1;
  }

  picoradar::mock_client::SyncClient client;
  std::string mode_str = argv[1];

  if (mode_str == "--discover") {
    if (argc != 3) {
      LOG(ERROR) << "Usage: " << argv[0] << " --discover <player_id>";
      return 1;
    }
    const std::string player_id = argv[2];
    return client.discover_and_run(player_id);
  } else {
    if (argc < 4) {
      LOG(ERROR) << "Usage: " << argv[0]
                 << " <host> <port> <player_id> [mode]";
      return 1;
    }
    const std::string host = argv[1];
    const std::string port = argv[2];
    const std::string player_id = argv[3];
    const std::string mode = (argc > 4) ? argv[4] : "--interactive";
    return client.run(host, port, mode, player_id);
  }

  return 0;
} 
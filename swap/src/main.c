#include <utils/net_utils.h>
#include <commons/config.h>

int main(int argc, char* argv[]) {

    t_log *logger;
    t_config *config;

    config = config_create("swap.config");

    if(config == NULL){
        abort();
    }

    char* ip = config_get_string_value(config, "IP");
    char* port = config_get_string_value(config, "PUERTO");

    logger = log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);
    log_info(logger, "SWAP started");

    int server = server_start(logger);

    log_info(logger, "SWAP ready to receive connections");

    int client = server_client_wait(server);

    log_info(logger, "Connected to Kernel Memory");

    log_destroy(logger);
    config_destroy(config);

    return 0;
}

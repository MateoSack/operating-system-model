#include <utils/net_utils.h>
#include <commons/config.h>

t_log *logger;

int main(int argc, char* argv[]) {

    t_config *config;
    int swap_fd;
    char *ip; 
    char *port;
    config = config_create("swap.config");

    if(config == NULL){
        abort();
    }

    ip = config_get_string_value(config, "IP");
    port = config_get_string_value(config, "PUERTO");

    logger = log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);
    log_info(logger, "SWAP started");

    swap_fd = connection_create(ip, port, logger);

    if(swap_fd == -1)
    {
        log_error(logger, "swap connection failed");
        return EXIT_FAILURE;
    }

    t_module_id_send(swap_fd, MODULE_SWAP, logger);
    log_info(logger, "Successful connection to kernel memory");

    log_destroy(logger);
    config_destroy(config);

    return 0;
}

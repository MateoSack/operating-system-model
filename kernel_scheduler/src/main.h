#include <utils/net_utils.h>
t_log *logger_start(void)
{
	t_log *new_logger;

	new_logger = log_create("kernel_schedul.log", "process_name", true, LOG_LEVEL_INFO);

	return new_logger;
}
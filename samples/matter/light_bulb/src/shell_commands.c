#include <zephyr/shell/shell.h>

static int cmd_dummy(const struct shell *shell, size_t argc, char **argv)
{
	printk("Something!\n");
	return 0;
}

SHELL_CMD_REGISTER(dummy, NULL, "Dummy command", cmd_dummy);

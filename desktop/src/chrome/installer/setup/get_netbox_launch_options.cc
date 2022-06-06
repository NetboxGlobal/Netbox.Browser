#include "base/command_line.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"

// netboxcode

base::CommandLine get_netbox_launch_options(const installer::InstallerState &installer_state, const base::CommandLine& cmd_line)
{
    base::CommandLine options(base::CommandLine::NO_PROGRAM);
    if (installer_state.verbose_logging())    
    {
        options.AppendSwitch(installer::switches::kEnableLogging);
        options.AppendSwitchASCII("v", "1");
    }

    if (cmd_line.HasSwitch(installer::switches::kNoStartupWindow))
    {
        options.AppendSwitch(installer::switches::kNoStartupWindow);
    }

    return options;
}
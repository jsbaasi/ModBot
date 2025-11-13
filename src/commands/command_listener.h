#ifndef COMMAND_LISTENER_H
#define COMMAND_LISTENER_H

#include <dpp/dpp.h>
    
class CommandListener {
    
public:
    static void on_slashcommand(const dpp::slashcommand_t& event);
};

#endif
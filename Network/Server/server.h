#ifndef SERVER_H
#define SERVER_H

#include <string>
#include "../Common/protocol.h"

void run_server(unsigned short port, const std::string& answer_word);

#endif // SERVER_H
#ifndef SERVER_H
#define SERVER_H

#include <string>
#include "../Common/protocol.h"

int max_Player = 2; // temporary value
int current_Player = 0;

void run_server(unsigned short port, const std::string& answer_word);

#endif // SERVER_H
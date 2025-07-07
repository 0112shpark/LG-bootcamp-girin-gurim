#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <map>
#include "../Common/protocol.h"

int max_Player = 2; // temporary value
int current_Player = 0;
std::map<std::string, int> player_scores;

void run_server(unsigned short port, const std::string& answer_word);

#endif // SERVER_H
#ifndef PROTOCOL_H
#define PROTOCOL_H
#define SERVER_IP "192.168.10.2"
#define SERVER_PORT 25000

#include <string>

#define MAX_CLIENTS 10

enum MessageType {
    MSG_DRAW = 1,
    MSG_CLEAR = 2,
    MSG_PING  = 3,
    MSG_ANSWER = 4,
    MSG_CORRECT = 5,
    MSG_WRONG = 6,
    MSG_PLAYER_NUM = 7,
    MSG_DISCONNECT = 8,
    MSG_PLAYER_CNT = 9
};

struct DrawPacket {
    int type;
    int x;
    int y;
    int color;
    int thick;
    int drawStatus;
};

struct AnswerPacket {
    int type;
    std::string nickname;
    std::string answer;
};

struct CorrectPacket {
    int type;
    std::string nickname;
};

struct WrongPacket {
    int type;
    std::string message;
    std::string nickname;
};

struct PlayerNumPacket {
    int type;
    int player_num;
};

struct PlayerCntPacket {
    int type;
    int currentPlayer_cnt;
    int maxPlayer;
};

#endif
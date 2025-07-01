#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_CLIENTS 10

enum MessageType {
    MSG_DRAW = 1,
    MSG_CLEAR = 2,
    MSG_PING  = 3
};

typedef struct {
    int type;
    int x;
    int y;
    int color;
    int thick;
} DrawPacket;

#endif // PROTOCOL_H

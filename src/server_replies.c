#include "server_replies.h"
#include "util.h"

#define REPLY(NUM, TEXT) { .code = (NUM), .text = #NUM " " TEXT "\r\n" }

smtp_reply replies[] = {
    REPLY(220, "%s Service ready"),
    REPLY(221, "%s Service closing transmission channel"),
    REPLY(250, "Ok"),
    REPLY(251, "User not local; will proceed to %s"),
    REPLY(354, "Start mail input; end with <CLRF>.<CLRF>"),
    REPLY(451, "Requested action aborted: error in processing"),
    REPLY(500, "Syntax error, command unrecognized"),
    REPLY(501, "Syntax error in parameters or arguments"),
    REPLY(502, "Command not implemented"),
    REPLY(503, "Bad sequence of commands"),
};


const char *get_server_reply(int code)
{
    for (unsigned int i = 0; i < ARRAYNUM(replies); i++)
    {
        smtp_reply *r = replies + i;
        
        if (r->code == code)
        {
            return r->text;
        }
    }
    
    return NULL;
}


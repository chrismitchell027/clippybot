#include "bot.h"
#include <fstream>

std::vector<std::pair<double,double>> miners;

int main()
{
    mpg123_init();
    Bot bot(dpp::intents::i_guild_members | dpp::intents::i_message_content);
    return 0;
}
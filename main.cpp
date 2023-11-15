#include "bot.h"

std::vector<std::pair<double,double>> miners;
std::vector<std::pair<std::string, std::string>> sounds;

int main()
{
    mpg123_init();
    Bot bot(dpp::intents::i_guild_members | dpp::intents::i_message_content);
    return 0;
}
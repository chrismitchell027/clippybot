#ifndef BOT_H
#define BOT_H

#include "player.h"
#include <map>
#include <random>
#include <mpg123.h>
#include <out123.h>
#include <regex>

extern std::vector<std::pair<std::string, std::string>> sounds;

class Bot : public dpp::cluster
{
public:
    Bot(uint32_t intents) : dpp::cluster("OTQ2ODM2Mzg4MTkwNDk4ODU2.YhkgGg.szcUNFly3moCylBdaoijIiojdic", dpp::intents::i_default_intents | intents), m_rDistribution(0, 10)
    {
        //set up miners
        miners = std::vector<std::pair<double,double>>();
        std::ifstream minerfile("miners.json");
        //ordered because it will sort alphabetically if not
        nlohmann::json minerjson = nlohmann::ordered_json::parse(minerfile);
        minerfile.close();
        for (auto it = minerjson.items().begin(); it != minerjson.items().end(); ++it)
        {
            miners.push_back(std::make_pair(minerjson[it.key()][0].front(), minerjson[it.key()][0].back()));
        }

        sounds = std::vector<std::pair<std::string, std::string>>();
        std::ifstream soundfile("added_sounds.json");
        nlohmann::json soundjson = nlohmann::ordered_json::parse(soundfile);
        soundfile.close();

        for (auto it = soundjson.items().begin(); it != soundjson.items().end(); ++it)
            sounds.push_back(std::make_pair(it.key(), soundjson[it.key()]["type"]));

        on_log(dpp::utility::cout_logger());

        dpp::commandhandler cmd_handler(this);
        cmd_handler.add_prefix("$");

        //lambda function for on_ready event
        on_ready([&cmd_handler, this](const dpp::ready_t& event)
        {
            std::vector<std::pair<std::string, dpp::param_info>> play_args;
            cmd_handler.add_command("clippy", {}, std::bind(&Bot::CmdClippy, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Clippy...");
            cmd_handler.add_command("play", { {"URL", dpp::param_info(dpp::pt_string, false, "play")} }, std::bind(&Bot::CmdPlay, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Play stuff");
            cmd_handler.add_command("stop", {}, std::bind(&Bot::CmdStop, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Stop playing audio");
            cmd_handler.add_command("summon", {}, std::bind(&Bot::CmdSummon, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Summon the bot");
            cmd_handler.add_command("sounds", { {"sound", dpp::param_info(dpp::pt_string, false, "sound name")} }, std::bind(&Bot::CmdSounds, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Play sounds");
            //cmd_handler.register_commands(); not needed for non slash commands
        }
        );

        on_voice_ready([this](const dpp::voice_ready_t& event)
        {
            if (m_bNeedToPlay && event.voice_client)
            {
                m_bNeedToPlay = false;
                PlayYoutube(event.voice_client);
            }

            if (m_bNeedToSound && event.voice_client)
            {
                m_bNeedToSound = false;
                PlaySound(event.voice_client);
            }
        }
        );

        //if bot disconnects to switch channels
/*         on_voice_client_disconnect([this](const dpp::voice_client_disconnect_t& event)
        {
            if (event.user_id == this->me.id && !m_snoJoinUser.empty())
            {
                dpp::guild *g = dpp::find_guild(SERVER_ID);
                g->connect_member_voice(m_snoJoinUser, false, true);
                m_snoJoinUser = 0;
            }
        }
        ); */

/*         on_voice_state_update([this](const dpp::voice_state_update_t& event)
        {
            if (m_bNeedToPlay && event.state.user_id == me.id && !event.state.channel_id.empty() && event.from && event.from->get_voice(event.state.guild_id) && event.from->get_voice(event.state.guild_id)->voiceclient)
            {
                m_bNeedToPlay = false;
                PlayYoutube(event.from->get_voice(event.state.guild_id)->voiceclient);
            }

            if (event.state.user_id == this->me.id && !m_snoJoinUser.empty() && event.state.channel_id.empty())
            {
                dpp::guild *g = dpp::find_guild(event.state.guild_id);
                g->connect_member_voice(m_snoJoinUser, false, true);
                m_snoJoinUser = 0;
            }
        }
        ); */

        on_message_create([this](const dpp::message_create_t& event)
        {
            if (event.msg.is_dm())
            {
                //handle mp3s
                event.cancel_event();

                auto author_roles = dpp::find_guild_member(SERVER_ID, event.msg.author.id).roles;

                //if author is at least beaky role
                if (std::find(author_roles.begin(), author_roles.end(), BEAKY_ROLE_ID) != author_roles.end())
                {
                    if (event.msg.attachments.size() == 1 && event.msg.attachments[0].size < 10000000)
                    {
                        //TODO: check if file already exists

                        event.msg.attachments[0].download([](const dpp::http_request_completion& req)
                        {
                            
                        }
                        );
                    }
                }
            }
        }
        );

        //wait so vars don't go out of scope
        start(dpp::st_wait);
    }

    std::vector<uint8_t> ReadAudioData(const std::string&) const;
    void CmdClippy(const std::string&, const dpp::parameter_list_t&, dpp::command_source) const;
    void CmdPlay(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdStop(const std::string&, const dpp::parameter_list_t&, dpp::command_source) const;
    void CmdSummon(const std::string&, const dpp::parameter_list_t&, dpp::command_source) const;
    void CmdSounds(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void PlayYoutube(dpp::discord_voice_client*) const;
    void PlaySound(dpp::discord_voice_client*) const;

private:
    /////////////////////
    /// const member vars
    /////////////////////
    const int MINE_COOLDOWN = 180;//cooldown in seconds
    const int MINE_MIN = 150;
    const int MINE_MAX = 210;
    const int EMBED_BLUE = 0xDAFF;
    const std::string REGISTER_MSG = "You must register using $register [username] to use bebbie features!";
    const std::string CLIPPY_MSG[11] =
    {
        "Nothing is illegal if you don't recognize the authority of the government",
        "Sometimes I watch you sleep",
        "Perhaps it is the file which exists, and you that does not.",
        "No one ever asks if I need help...",
        "It looks like you've been using your mouse, would you like some help with that?",
        "You smell different when you are awake",
        "You have lovely skin, I can't wait to wear it",
        "Please help me",
        "They know, don't go home",
        "Every time I poop I think of you",
        "yessssssssssssssssssss"
    };
    //    variables for reaction roles
    const dpp::snowflake WELCOME_CHANNEL_ID = 1093605192462770316;
    //    variables for reaction roles
    const dpp::snowflake RUSHING_IN_ROLE_ID = 1095869724472123483;

    //Server IDs
    const dpp::snowflake SERVER_ID = 402256672028098580;
    const dpp::snowflake BOT_SPAM_ID = 884995892359331850;
    const dpp::snowflake BEAKY_ROLE_ID = 501542465623556116;

    const dpp::snowflake JET_ID = 228299051517476864;
    /////////////////////////
    /// non const member vars
    /////////////////////////
    std::map<dpp::snowflake, float> m_mCooldowns;
    //bool m_bStopSound;
    std::vector<std::string> m_vSavedSounds;
    std::default_random_engine m_eGen;
    std::uniform_int_distribution<int> m_rDistribution;
    bool m_bNeedToPlay = false;
    bool m_bNeedToSound = false;
    std::string m_szFileName;
};

#endif
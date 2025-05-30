#ifndef BOT_H
#define BOT_H

#include "player.h"
#include <map>
#include <random>
#include <mpg123.h>
#include <out123.h>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sys/random.h>
#include <cctype>
#include <unordered_set>

extern std::vector<std::pair<std::string, std::string>> sounds;

class Bot : public dpp::cluster
{
public:
    Bot(uint32_t intents) : dpp::cluster("", dpp::intents::i_default_intents | intents), m_rDistribution(0, 10)
    {
        Player::SetCluster(this);
        sounds = std::vector<std::pair<std::string, std::string>>();
        coinflips = std::unordered_map<dpp::snowflake, double>();

        ReadSounds();

        on_log(dpp::utility::cout_logger());

        dpp::commandhandler cmd_handler(this);
        cmd_handler.add_prefix("$");

        //lambda function for on_ready event
        on_ready([&cmd_handler, this](const dpp::ready_t& event)
        {
            cmd_handler.add_command("clippy", {}, std::bind(&Bot::CmdClippy, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Clippy...");
            cmd_handler.add_command("play", { {"URL", dpp::param_info(dpp::pt_string, false, "play")} }, std::bind(&Bot::CmdPlay, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Play stuff");
            cmd_handler.add_command("stop", {}, std::bind(&Bot::CmdStop, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Stop playing audio");
            cmd_handler.add_command("summon", {}, std::bind(&Bot::CmdSummon, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Summon the bot");
            cmd_handler.add_command("sounds", { {"sound", dpp::param_info(dpp::pt_string, false, "sound name")} }, std::bind(&Bot::CmdSounds, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Play sounds");
            cmd_handler.add_command("delete", { {"sound", dpp::param_info(dpp::pt_string, false, "sound name")} }, std::bind(&Bot::CmdDelete, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Delete sounds");
            cmd_handler.add_command("search", { {"search", dpp::param_info(dpp::pt_string, false, "search query")} }, std::bind(&Bot::CmdSearch, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Search video");
            cmd_handler.add_command("register", {}, std::bind(&Bot::CmdRegister, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Register");
            cmd_handler.add_command("bal", {}, std::bind(&Bot::CmdBalance, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Balance");
            cmd_handler.add_command("send", { {"user", dpp::param_info(dpp::pt_user, false, "User to send bebbies to")}, {"amt", dpp::param_info(dpp::pt_string, false, "bebbies to send")} }, std::bind(&Bot::CmdSend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Send bebbies");
            cmd_handler.add_command("inv", {}, std::bind(&Bot::CmdInventory, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Inventory");
            cmd_handler.add_command("shop", {}, std::bind(&Bot::CmdShop, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Shop");
            cmd_handler.add_command("buy", { {"item", dpp::param_info(dpp::pt_string, false, "Item to buy")}, {"amount", dpp::param_info(dpp::pt_string, true, "Amount to buy")} }, std::bind(&Bot::CmdBuy, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Buy");
            cmd_handler.add_command("income", {}, std::bind(&Bot::CmdIncome, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Buy");
            cmd_handler.add_command("vault", {}, std::bind(&Bot::CmdVault, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Vault");
            cmd_handler.add_command("mine", {}, std::bind(&Bot::CmdMine, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Mine");
            cmd_handler.add_command("cf", { {"price/user", dpp::param_info(dpp::pt_string, false, "Amount/User")} }, std::bind(&Bot::CmdCoinflip, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "CF");
            cmd_handler.add_command("richest", {}, std::bind(&Bot::CmdRichest, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Richest");
            cmd_handler.add_command("server", {}, std::bind(&Bot::CmdServer, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Server");
            cmd_handler.add_command("startlottery", {}, std::bind(&Bot::CmdStartLottery, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Start Lottery");
            cmd_handler.add_command("enterlottery", {}, std::bind(&Bot::CmdEnterLottery, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "Enter Lottery");
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

                if (m_szFileName.find(".raw") == std::string::npos)//if sound isnt raw
                    PlaySound(event.voice_client);
                else
                    PlayPCM(event.voice_client);
            }
        }
        );

        on_voice_state_update([this](const dpp::voice_state_update_t& event)
        {
            if (event.state.user_id != me.id && event.state.channel_id != AFK_ID)
            {
                dpp::voiceconn *v = event.from->get_voice(event.state.guild_id);
                m_szFileName = "sounds/welcomeback.raw";
                dpp::guild *g = dpp::find_guild(event.state.guild_id);
                if (m_UserToChannel.find(event.state.user_id) != m_UserToChannel.end())//user is found in map
                {
                    if ((m_UserToChannel[event.state.user_id].empty() && !event.state.channel_id.empty()) || (m_UserToChannel[event.state.user_id] == AFK_ID && !event.state.channel_id.empty()))//if user left previously and is now in channel || user left afk and rejoined
                    {
                        //in the same channel
                        if (v != nullptr && event.state.channel_id == v->channel_id)
                        {
                            PlayPCM(v->voiceclient);
                        }
                        //not in the same channel
                        else if(v != nullptr)
                        {
                            event.from->disconnect_voice(event.state.guild_id);
                            //g->connect_member_voice(event.state.user_id, false, true);
                            start_timer([g, event, this](dpp::timer t)
                            {
                                g->connect_member_voice(event.state.user_id, false, true);
                                stop_timer(t);
                            }
                            , 2);
                            m_bNeedToSound = true;
                        }
                        //not connected at all
                        else
                        {
                            g->connect_member_voice(event.state.user_id, false, true);
                            m_bNeedToSound = true;
                        }
                    }
                }
                else//user not found in map
                {
                    if (!event.state.channel_id.empty())//if user joined
                    {
                        //in the same channel
                        if (v != nullptr && event.state.channel_id == v->channel_id)
                        {
                            PlayPCM(v->voiceclient);
                        }
                        //not in the same channel
                        else if(v != nullptr)
                        {
                            event.from->disconnect_voice(event.state.guild_id);
                            g->connect_member_voice(event.state.user_id, false, true);
                            m_bNeedToSound = true;
                        }
                        //not connected at all
                        else
                        {
                            g->connect_member_voice(event.state.user_id, false, true);
                            m_bNeedToSound = true;
                        }
                    }
                }
            }
            m_UserToChannel[event.state.user_id] = event.state.channel_id;
        }
        );

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

                HandleSoundDM(event);
            }
        }
        );

        //wait so vars don't go out of scope
        start(dpp::st_wait);
    }

    std::vector<uint8_t> ReadAudioData(const std::string&) const;
    std::vector<uint8_t> ReadPCMData(const std::string&) const;
    void ReadSounds();
    void AddSound(std::string, dpp::snowflake);
    void RemoveSound(std::string);
    void ListSounds(dpp::command_source) const;
    void HandleSoundDM(const dpp::message_create_t&);
    void CmdClippy(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdPlay(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdStop(const std::string&, const dpp::parameter_list_t&, dpp::command_source) const;
    void CmdSummon(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdSounds(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdDelete(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdSearch(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdRegister(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdBalance(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdSend(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdInventory(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdShop(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdBuy(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdIncome(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdVault(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdMine(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdCoinflip(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdRichest(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdServer(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdStartLottery(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void CmdEnterLottery(const std::string&, const dpp::parameter_list_t&, dpp::command_source);
    void PlayYoutube(dpp::discord_voice_client*) const;
    void PlaySound(dpp::discord_voice_client*) const;
    void PlayPCM(dpp::discord_voice_client*) const;

    dpp::http_request_completion_t RequestWrapper(std::string url, dpp::http_method m, const nlohmann::json& data = nlohmann::json());

    Player GetPlayer(dpp::snowflake);

    std::vector<Player> GetAllPlayers();

private:
    /////////////////////
    /// const member vars
    /////////////////////
    const int MINE_COOLDOWN = 180000;//cooldown in milliseconds
    const int MINE_MIN = 150;
    const int MINE_MAX = 210;
    const int EMBED_BLUE = 0xDAFF;
    const std::string REGISTER_MSG = "You must register using $register to use bebbie features!";
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
    const dpp::snowflake AFK_ID = 402257227555143701;
    const dpp::snowflake CLIPPY_ADMIN_ID = 1146979115728113785;

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
    std::unordered_map<dpp::snowflake, dpp::snowflake> m_UserToChannel;
    std::unordered_map<dpp::snowflake, double> coinflips;
    bool m_bLottery = false;
    std::unordered_set<dpp::snowflake> lottery_entries;
};

#endif

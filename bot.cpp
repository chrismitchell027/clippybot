#include "bot.h"

#define BOT_SPAM_CHECK if (cs.channel_id == BOT_SPAM_ID)
#define CLIPPY_ADMIN_CHECK if (cs.channel_id == CLIPPY_ADMIN_ID)

std::string ThousandsFormat(double n)
{
    std::stringstream ss;
    ss.imbue(std::locale("en_US.UTF-8"));
    ss << std::fixed << std::setprecision(1) << n;
    return ss.str();
}

/**
 * Reads audio data and puts it into vector,
 * taken from https://dpp.dev/stream-mp3-discord-bot.html
 * @return Vector with audio data
*/
std::vector<uint8_t> Bot::ReadAudioData(const std::string& file_dir) const
{
    std::vector<uint8_t> pcmdata;

    int err = 0;
    unsigned char* buffer;
    size_t buffer_size, done;
    int channels, encoding;
    long rate;

    /* Note it is important to force the frequency to 48000 for Discord compatibility */
    mpg123_handle *mh = mpg123_new(NULL, &err);
    mpg123_param(mh, MPG123_FORCE_RATE, 48000, 48000.0);

    /* Decode entire file into a vector. You could do this on the fly, but if you do that
    * you may get timing issues if your CPU is busy at the time and you are streaming to
    * a lot of channels/guilds.
    */
    buffer_size = mpg123_outblock(mh);
    buffer = new unsigned char[buffer_size];

    /* Note: In a real world bot, this should have some error logging */
    mpg123_open(mh, file_dir.c_str());
    mpg123_getformat(mh, &rate, &channels, &encoding);

    unsigned int counter = 0;
    for (int totalBytes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) {
        for (size_t i = 0; i < buffer_size; i++) {
            pcmdata.push_back(buffer[i]);
        }
        counter += buffer_size;
        totalBytes += done;
    }
    delete[] buffer;
    mpg123_close(mh);
    mpg123_delete(mh);
    return pcmdata;
}

std::vector<uint8_t> Bot::ReadPCMData(const std::string& file_dir) const
{
    std::fstream pcm_file = std::fstream(file_dir, std::fstream::in | std::fstream::binary);
    std::vector<uint8_t> pcm_data;
    uint8_t buf;
    auto size = std::filesystem::file_size(file_dir);

    for (uintmax_t i = 0; i < size; i++)
    {
        pcm_file.read((char*)&buf, 1);
        pcm_data.push_back(buf);
    }

    return pcm_data;
}

void Bot::ReadSounds()
{
    sounds.clear();

    std::ifstream soundfile("added_sounds.json");
    nlohmann::json soundjson = nlohmann::ordered_json::parse(soundfile);

    for (auto it = soundjson.items().begin(); it != soundjson.items().end(); ++it)
        sounds.push_back(std::make_pair(it.key(), soundjson[it.key()]["type"]));
}

void Bot::AddSound(std::string sound, dpp::snowflake author)
{
    std::ifstream soundfile("added_sounds.json");
    nlohmann::json soundjson = nlohmann::ordered_json::parse(soundfile);
    soundfile.close();

    soundjson[sound]["type"] = "raw";
    soundjson[sound]["author"] = (uint64_t)author;

    std::fstream soundfile_write("added_sounds.json", std::fstream::out | std::fstream::trunc);
    soundfile_write << soundjson.dump(4);
}

void Bot::RemoveSound(std::string sound)
{
    std::ifstream soundfile("added_sounds.json");
    nlohmann::json soundjson = nlohmann::ordered_json::parse(soundfile);
    soundfile.close();
    
    soundjson.erase(sound);

    std::fstream soundfile_write("added_sounds.json", std::fstream::out | std::fstream::trunc);
    soundfile_write << soundjson.dump(4);
}

void Bot::ListSounds(dpp::command_source cs) const
{
    int sounds_page_num = 1;
    int sounds_added_count = 0;
    dpp::embed sounds_embed;
    sounds_embed.title = std::format("Sounds Page {}", sounds_page_num);
    sounds_embed.color = 0x00DAFF;

    for (auto s : sounds)
    {
        sounds_embed.add_field(s.first, "", true);
        sounds_added_count++;
        if (sounds_added_count == 25)
        {
            cs.message_event.value().send(dpp::message(cs.channel_id, sounds_embed));
            sounds_page_num++;
            sounds_embed = dpp::embed();
            sounds_embed.title = std::format("Sounds Page {}", sounds_page_num);
            sounds_embed.color = 0x00DAFF;
            sounds_added_count = 0;
        }
    }

    if (sounds_added_count > 0)
        cs.message_event.value().send(dpp::message(cs.channel_id, sounds_embed));
}

void Bot::HandleSoundDM(const dpp::message_create_t& event)
{
    auto author_roles = dpp::find_guild_member(SERVER_ID, event.msg.author.id).get_roles();

    //if author is at least beaky role
    if (std::find(author_roles.begin(), author_roles.end(), BEAKY_ROLE_ID) != author_roles.end())
    {
        if (event.msg.attachments.size() == 1 && event.msg.attachments[0].size < 10000000)
        {
            
            std::string filename = event.msg.attachments[0].filename;
            bool file_exists = std::filesystem::exists(std::format("sounds/saved_sounds/{}", filename));

            if (filename.substr(filename.find_last_of('.')) != std::string(".mp3"))
            {
                event.reply("Sound must be an mp3");
                return;
            }

            if (filename.length() > 29)
            {
                event.reply("Filename too long, must be <= 25 characters");
                return;
            }

            std::regex file("([a-z0-9_]{1,25}\\.mp3)");
            std::smatch match;
            if (!std::regex_search(filename, match, file) || match.str() != filename)
            {
                event.reply("Invalid filename");
                return;
            }

            if (file_exists)
            {
                event.reply(std::format("Sound {} already exists", filename));
                return;
            }

            event.reply(std::format("{} successfully added!", filename));

            AddSound(filename.substr(0, filename.find_last_of('.')), event.msg.author.id);
            ReadSounds();

            event.msg.attachments[0].download([filename](const dpp::http_request_completion_t& req)
            {
                std::fstream mp3(std::format("sounds/saved_sounds/{}", filename), std::fstream::out | std::fstream::binary);
                
                mp3.write(req.body.c_str(), req.body.size());
                mp3.close();
                system(std::format("ffmpeg -i sounds/saved_sounds/{0}.mp3 -f s16le -acodec pcm_s16le -ar 48000 -ac 2 sounds/saved_sounds/{0}.raw", filename.substr(0, filename.find(".mp3"))).c_str());
                std::filesystem::remove(std::format("sounds/saved_sounds/{}", filename));
            }
            );
        }
    }
}

void Bot::CmdClippy(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK 
        cs.message_event.value().reply(CLIPPY_MSG[m_rDistribution(m_eGen)]);
}

void Bot::PlayYoutube(dpp::discord_voice_client *vc) const
{
    auto sound_data = ReadAudioData("yt.mp3");

    //Don't need mp3 anymore, it is stored in sound_data now
    std::filesystem::remove("yt.mp3");

    if (vc)
    {
        vc->send_audio_raw((uint16_t*)sound_data.data(), sound_data.size());
    }
}

void Bot::PlaySound(dpp::discord_voice_client *vc) const
{
    auto sound_data = ReadAudioData(m_szFileName);

    if (vc)
        vc->send_audio_raw((uint16_t*)sound_data.data(), sound_data.size());
}

void Bot::PlayPCM(dpp::discord_voice_client *vc) const
{
    auto pcm_data = ReadPCMData(m_szFileName);

    if (vc)
        vc->send_audio_raw((uint16_t*)pcm_data.data(), pcm_data.size());
}

void Bot::CmdPlay(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        std::string param = std::get<std::string>(param_list[0].second);
        if (param.empty())
        {
            cs.message_event.value().reply("URL required for $play");
            return;
        }
        std::regex link("([A-Z0-9_-]){11}", std::regex_constants::icase);
        std::smatch match;
        if (!std::regex_search(param, match, link))
        {
            cs.message_event.value().reply("Invalid URL");
            return;
        }

        dpp::guild *g = dpp::find_guild(cs.guild_id);
        dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);

        system(std::format("yt-dlp -x --audio-format mp3 https://www.youtube.com/watch?v={} -o yt.mp3", match.str()).c_str());

        //in the same channel
        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
        {
            PlayYoutube(v->voiceclient);
        }
        //not in the same channel
        else if(v != nullptr)
        {
            cs.message_event.value().from->disconnect_voice(cs.guild_id);
            //g->connect_member_voice(cs.issuer.id, false, true);

            start_timer([g, cs, this](dpp::timer t)
            {
                g->connect_member_voice(cs.issuer.id, false, true);
                stop_timer(t);
            }
            , 2);
            m_bNeedToPlay = true;
        }
        //not connected at all
        else
        {
            g->connect_member_voice(cs.issuer.id, false, true);
            m_bNeedToPlay = true;
        }
    }
}

void Bot::CmdSearch(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        std::string param = std::get<std::string>(param_list[0].second);
        if (param.empty())
        {
            cs.message_event.value().reply("Search query required for $search");
            return;
        }
        std::regex link("([A-Z0-9_\\s]){1,20}", std::regex_constants::icase);
        std::smatch match;
        if (!std::regex_search(param, match, link))
        {
            cs.message_event.value().reply("Invalid search");
            return;
        }

        dpp::guild *g = dpp::find_guild(cs.guild_id);
        dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);

        system(std::format("yt-dlp -x --audio-format mp3 \"ytsearch:{}\" -o yt.mp3", match.str()).c_str());

        //in the same channel
        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
        {
            PlayYoutube(v->voiceclient);
        }
        //not in the same channel
        else if(v != nullptr)
        {
            cs.message_event.value().from->disconnect_voice(cs.guild_id);
            //g->connect_member_voice(cs.issuer.id, false, true);

            start_timer([g, cs, this](dpp::timer t)
            {
                g->connect_member_voice(cs.issuer.id, false, true);
                stop_timer(t);
            }
            , 2);
            m_bNeedToPlay = true;
        }
        //not connected at all
        else
        {
            g->connect_member_voice(cs.issuer.id, false, true);
            m_bNeedToPlay = true;
        }
    }
}

void Bot::CmdStop(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);
        if (v && v->voiceclient)
            v->voiceclient->stop_audio();
    }
}

void Bot::CmdSummon(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        dpp::guild *g = dpp::find_guild(cs.guild_id);
        dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);

        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id != v->channel_id)
        {
            cs.message_event.value().from->disconnect_voice(cs.guild_id);
            //g->connect_member_voice(cs.issuer.id, false, true);
            start_timer([g, cs, this](dpp::timer t)
            {
                g->connect_member_voice(cs.issuer.id, false, true);
                stop_timer(t);
            }
            , 2);
        }
        else if (v == nullptr)
            g->connect_member_voice(cs.issuer.id, false, true);

        for (auto vs : g->voice_members)
        {
            m_UserToChannel[vs.second.user_id] = vs.second.channel_id;
        }
    }
}

void Bot::CmdSounds(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        std::string param = std::get<std::string>(param_list[0].second);
        if (param.empty())
        {
            ListSounds(cs);
            return;
        }

        if (cs.issuer.id == JET_ID && (param == "sop" || param == "bb" || param == "bcs" || param == "fuckyou"))// # jet is not allowed to play loud sounds
        {
            cs.message_event.value().reply("buhhhh no");
            return;
        }

        for (auto s : sounds)
            if (s.first == param)
            {
                m_szFileName = std::format("sounds/saved_sounds/{}.{}", s.first, s.second);
                if (!std::filesystem::exists(m_szFileName))
                {
                    cs.message_event.value().reply(std::format("Error: {}.{} doesn't exist", s.first, s.second));
                    return;
                }

                dpp::guild *g = dpp::find_guild(cs.guild_id);
                dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);

                //in the same channel
                if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
                    PlayPCM(v->voiceclient);
                //not in the same channel
                else if(v != nullptr)
                {
                    cs.message_event.value().from->disconnect_voice(cs.guild_id);
                    //g->connect_member_voice(cs.issuer.id, false, true);
                    start_timer([g, cs, this](dpp::timer t)
                    {
                        g->connect_member_voice(cs.issuer.id, false, true);
                        stop_timer(t);
                    }
                    , 2);
                    m_bNeedToSound = true;
                }
                //not connected at all
                else
                {
                    g->connect_member_voice(cs.issuer.id, false, true);
                    m_bNeedToSound = true;
                }

                return;
            }
        
        cs.message_event.value().reply(std::format("Sound {} doesn't exist", param));
    }
}

void Bot::CmdDelete(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    CLIPPY_ADMIN_CHECK
    {
        std::string param = std::get<std::string>(param_list[0].second);
        if (param.empty())
        {
            cs.message_event.value().reply("Sound name required");
            return;
        }

        for (auto s : sounds)
            if (s.first == param)
            {
                m_szFileName = std::format("sounds/saved_sounds/{}.{}", s.first, s.second);
                if (!std::filesystem::exists(m_szFileName))
                {
                    cs.message_event.value().reply(std::format("Error: {}.{} doesn't exist", s.first, s.second));
                    return;
                }

                RemoveSound(s.first);
                ReadSounds();
                std::filesystem::remove(m_szFileName);

                cs.message_event.value().reply(std::format("Sound {} deleted", s.first));

                return;
            }
        cs.message_event.value().reply(std::format("Sound {} doesn't exist", param));
    }
}

void Bot::CmdRegister(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        pqxx::work W(conn);
        try
        {
            W.exec_prepared("insert_user", (uint64_t)cs.issuer.id, 0, std::vector<int>(11, 0));
            W.commit();
        }
        catch (const pqxx::unique_violation& e)
        {

            cs.message_event.value().reply("You are already registered");
            W.abort();
            return;
        }

        cs.message_event.value().reply("You have been registered successfully");
        AddPlayer(Player((uint64_t)cs.issuer.id, 0, dpp::find_guild_member(SERVER_ID, cs.issuer.id).get_nickname()));
    }
}

void Bot::CmdBalance(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        for (const auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                cs.message_event.value().reply(std::format("You have {} bebbies", ThousandsFormat(p.GetBalance())));
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdSend(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        if (!(std::holds_alternative<dpp::resolved_user>(param_list[0].second) && std::holds_alternative<double>(param_list[1].second)))
        {
            cs.message_event.value().reply("Usage: $send @someone bebbie_amount");
            return;
        }
        dpp::guild_member m = std::get<dpp::resolved_user>(param_list[0].second).member;
        double amt = std::get<double>(param_list[1].second);
        if (amt <= 0.0)
        {
            cs.message_event.value().reply("Cannot send non-positive bebbie amount");
            return;
        }
        if (cs.issuer.id == m.user_id)
        {
            cs.message_event.value().reply("Cannot send bebbies to yourself");
            return;
        }
        for (auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                for (auto& r : players)
                {
                    if (r.GetUserID() == m.user_id)
                    {
                        if (p.GetBalance() >= amt)
                        {
                            p.AddBalance(-amt);
                            r.AddBalance(amt);
                            cs.message_event.value().reply(std::format("{} has sent {} {:.2f} bebbies", p.GetUsername(), r.GetUsername(), amt));
                            return;
                        }
                        else
                        {
                            cs.message_event.value().reply("You don't have enough bebbies to send");
                            return;
                        }
                    }
                }

                cs.message_event.value().reply("Recipient does not have an account");
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdInventory(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        for (const auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                dpp::embed inventoryEmbed;
                inventoryEmbed.title = "Inventory";
                inventoryEmbed.color = 0x00DAFF;

                for (int i = 0; i < p.GetInventory().size(); i++)
                    inventoryEmbed.add_field(MINERS[i], std::format("{} Owned", p.GetInventoryItem(i)), true);


                cs.message_event.value().send(dpp::message(cs.channel_id, inventoryEmbed));
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdShop(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        for (const auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                dpp::embed inventoryEmbed;
                inventoryEmbed.title = std::format("Bebbies Shop for {}", p.GetUsername());
                inventoryEmbed.color = 0x00DAFF;
                int ItemID = 0;

                for (int i = 0; i < p.GetInventory().size(); i++)
                {
                    inventoryEmbed.add_field(
                    std::format("Tier {} [{} Owned]", ItemID + 1, p.GetInventoryItem(i)), 
                    std::format("{}\nProduction: {} per second\nCost: {} bebbies", MINERS[i], ThousandsFormat(miners[i].second), ThousandsFormat(p.GetPrice(i))), 
                    true);
                    ItemID++;
                }


                cs.message_event.value().send(dpp::message(cs.channel_id, inventoryEmbed));
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdBuy(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        if (!std::holds_alternative<std::string>(param_list[0].second) || std::get<std::string>(param_list[0].second).empty())
        {
            cs.message_event.value().reply("Usage: $buy miner_id {amount of miners to buy}");
            return;
        }
        auto ItemID = std::stoi(std::get<std::string>(param_list[0].second)) - 1;
        if (ItemID < 0 || ItemID > 10)
        {
            cs.message_event.value().reply("Item ID is out of bounds");
            return;
        }

        if (std::holds_alternative<std::string>(param_list[1].second) && !std::get<std::string>(param_list[1].second).empty())
        {
            auto amount = std::stoi(std::get<std::string>(param_list[1].second));
            if (amount > 100)
            {
                cs.message_event.value().reply(std::format("Cannot buy over 100 miners"));
                return;
            }
            double totalPrice = 0;
            for (auto&p : players)
            {
                if (p.GetUserID() == cs.issuer.id)
                {
                    for (int i = 0; i < amount; i++)
                        totalPrice += p.GetPrice(ItemID, i + 1 + p.GetInventoryItem(ItemID));

                    if (p.GetBalance() >= totalPrice)
                    {
                        for (int i = 0; i < amount; i++)
                            p.BuyItem(ItemID);
                        
                        cs.message_event.value().reply(std::format("{} bought {} {}'s for {}", p.GetUsername(), amount, MINERS[ItemID], ThousandsFormat(totalPrice)));
                        return;
                    }
                    else
                    {
                        cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford {} {}'s for {}", p.GetUsername(), amount, MINERS[ItemID], ThousandsFormat(totalPrice)));
                        return;
                    }
                }
            }
        }
        else
        {
            for (auto& p : players)
            {
                if (p.GetUserID() == cs.issuer.id)
                {
                    auto itemPrice = p.GetPrice(ItemID);
                    if (p.GetBalance() >= itemPrice)
                    {
                        p.BuyItem(ItemID);
                        cs.message_event.value().reply(std::format("{} bought {} for {}", p.GetUsername(), MINERS[ItemID], ThousandsFormat(itemPrice)));
                    }
                    else
                        cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford a {} for {}", p.GetUsername(), MINERS[ItemID], ThousandsFormat(itemPrice)));
                    return;
                }
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdIncome(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        for (const auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                cs.message_event.value().reply(std::format("{} is currently mining {} bebbies per second", p.GetUsername(), ThousandsFormat(p.GetIncome())));
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdVault(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        dpp::embed vaultEmbed;
        vaultEmbed.title = "Bebbies Vault";
        vaultEmbed.color = 0x00DAFF;
        int i = 0;
        for (const auto& p : players)
        {
            i++;
            vaultEmbed.add_field(
            std::format("{}", i), 
            std::format("{} has {} bebbies", p.GetUsername(), ThousandsFormat(p.GetBalance())),
            true);

        }
        cs.message_event.value().send(dpp::message(cs.channel_id, vaultEmbed));
    }
}

void Bot::CmdMine(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        for (auto& p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (millis - p.GetCooldown() >= MINE_COOLDOWN)
                {
                    std::random_device rd;
                    std::mt19937 eng(rd());
                    auto amt = std::uniform_int_distribution<>(150, 210)(eng);
                    p.SetCooldown(millis);
                    p.AddBalance(amt);
                    cs.message_event.value().reply(std::format("you mined {} bebbies {}", amt, p.GetUsername()));
                }
                else
                {
                    cs.message_event.value().reply(std::format("too soon man, you gotta wait {:.1f} seconds to mine again.", double(MINE_COOLDOWN - (millis - p.GetCooldown())) / 1000.0));
                }
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdCoinflip(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        if (!std::holds_alternative<std::string>(param_list[0].second) || std::get<std::string>(param_list[0].second).empty())
        {
            dpp::embed cfEmbed;
            cfEmbed.title = "Coinflips";
            cfEmbed.color = 0x00DAFF;
            for (const auto& c : coinflips)
            {
                cfEmbed.add_field(
                std::format("{}", c.first->GetUsername()), 
                std::format("{} bebbies", ThousandsFormat(c.second)),
                true);
    
            }
            cs.message_event.value().send(dpp::message(cs.channel_id, cfEmbed));
            cs.message_event.value().reply("Usage: $cf bebbies/user");
            return;
        }
        auto value = std::get<std::string>(param_list[0].second);

        if (value == "stats")
        {
            for (const auto& p : players)
            {
                if (p.GetUserID() == cs.issuer.id)
                {
                    cs.message_event.value().reply(std::format("Coinflip stats for {}: {} wins and {} losses, {:.2f}% win rate, {} bebbies profit", p.GetUsername(), p.GetCfWins(), p.GetCfLosses(),  (double)(p.GetCfWins() * 100) / (p.GetCfWins() + p.GetCfLosses()), ThousandsFormat(p.GetCfProfit())));
                    return;
                }
            }
            cs.message_event.value().reply(REGISTER_MSG);
            return;
        }
        else if (value == "cancel")
        {
            for (auto &p : players)
            {
                if (p.GetUserID() == cs.issuer.id)
                {
                    if (coinflips.find(&p) != coinflips.end())
                    {
                        auto cfValue = coinflips[&p];
                        p.AddBalance(cfValue);
                        cs.message_event.value().reply(std::format("Coinflip cancelled, {} bebbies refunded", ThousandsFormat(cfValue)));
                        coinflips.erase(&p);
                        return;
                    }
                    else
                    {
                        cs.message_event.value().reply("No coinflip found");
                        return;
                    }
                }
            }
            cs.message_event.value().reply(REGISTER_MSG);
            return;
        }
        else
        {
            if (value.substr(0, 2) == "<@")//if performing the actual cf
            {
                for (auto& p : players)
                {
                    if (p.GetUserID() == cs.issuer.id)//if this is command sender
                    {
                        for (auto& c : coinflips)
                        {
                            if (c.first->GetUserID() == std::stol(value.substr(2, 18)) && std::stol(value.substr(2, 18)) != p.GetUserID())
                            {
                                if (p.GetBalance() >= c.second)
                                {
                                    /* std::random_device rd;
                                    std::mt19937 eng(rd());
                                    auto cfValue = std::uniform_int_distribution<>(0, 1)(eng); */
                                    int cfValue;
                                    getrandom(&cfValue, 4, 0);
                                    if (cfValue < 0)
                                        cfValue = 0;
                                    else
                                        cfValue = 1;

                                    cs.message_event.value().reply(std::format("{} won the coinflip!", cfValue == 0 ? p.GetUsername() : c.first->GetUsername()));
                                    if (cfValue == 0)
                                    {
                                        p.AddBalance(c.second);
                                        p.AddCfProfit(c.second);
                                        p.AddCfWin();
                                        c.first->AddCfLoss();
                                        c.first->AddCfProfit(-c.second);
                                    }
                                    else
                                    {
                                        p.AddBalance(-c.second);
                                        p.AddCfProfit(-c.second);
                                        p.AddCfLoss();
                                        c.first->AddBalance(2 * c.second);
                                        c.first->AddCfProfit(c.second);
                                        c.first->AddCfWin();
                                    }
                                    coinflips.erase(c.first);
                                    return;
                                }
                                else
                                {
                                    cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford the coinflip", p.GetUsername()));
                                    return;
                                }
                            }
                        }
                        cs.message_event.value().reply("No coinflip found");
                        return;
                    }
                }
            }
            else//if putting the cf up
            {
                if (!isdigit(value[0]))
                {
                    cs.message_event.value().reply(std::format("Invalid input"));
                    return;
                }
                //std::remove(value.begin(), value.end(), ',');

                auto amount = std::stod(value);

                auto lastChar = tolower(value.back());
                if (lastChar == 'k' || lastChar == 'm' || lastChar == 'b' || lastChar == 't')
                {
                    switch (lastChar)
                    {
                        case 'k':
                        {
                            amount *= 1000;
                            break;
                        }
                        case 'm':
                        {
                            amount *= 1000000;
                            break;
                        }
                        case 'b':
                        {
                            amount *= 1000000000;
                            break;
                        }
                        case 't':
                        {
                            amount *= 1000000000000;
                            break;
                        }
                    }
                }

                if (amount < 0)
                {
                    cs.message_event.value().reply(std::format("Cannot do negative coinflips"));
                    return;
                }

                for (auto& p : players)
                {
                    if (p.GetUserID() == cs.issuer.id)
                    {
                        if (coinflips.find(&p) != coinflips.end())
                        {
                            cs.message_event.value().reply(std::format("Cannot have more than one coinflip up", p.GetUsername()));
                            return;
                        }
                        if (p.GetBalance() >= amount)
                        {
                            p.AddBalance(-amount);
                            cs.message_event.value().reply(std::format("{} put up a coinflip for {}", p.GetUsername(), ThousandsFormat(amount)));
                            coinflips[&p] = amount;
                            return;
                        }
                        else
                            cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford the coinflip", p.GetUsername()));

                        return;
                    }
                }

            }
        }

        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdRichest(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs) const
{
    BOT_SPAM_CHECK
    {
        dpp::embed richestEmbed;
        richestEmbed.title = "Richest Players";
        richestEmbed.color = 0x00DAFF;
        int i = 0;
        std::vector<Player> richestPlayers = players;
        std::sort(richestPlayers.begin(), richestPlayers.end(), [](const Player& a, const Player& b)
        {
            return a.GetBalance() > b.GetBalance();
        });
        for (const auto& p : richestPlayers)
        {
            i++;
            if (i == 6) break;
            richestEmbed.add_field(
            std::format("{}", i), 
            std::format("{} has {} bebbies", p.GetUsername(), ThousandsFormat(p.GetBalance())),
            true);

        }
        cs.message_event.value().send(dpp::message(cs.channel_id, richestEmbed));
    }
}

void Bot::InitializePlayers()
{
    pqxx::work W(conn);
    pqxx::result R = W.exec_prepared("get_all_users");

    for (const auto& row : R)
    {
        std::vector<int> tmp;
        std::string inv_string = row[2].as<std::string>().substr(1, row[2].as<std::string>().size() - 2);
        std::stringstream ss(inv_string);
        std::string item;

        while (std::getline(ss, item, ','))
            tmp.push_back(std::stoi(item));

        AddPlayer(Player(row[0].as<int64_t>(), row[1].as<double>(), this->guild_get_member_sync(SERVER_ID, dpp::snowflake(row[0].as<int64_t>())).get_nickname(), tmp, row[3].as<int>(), row[4].as<int>(), row[5].as<double>()));
    }
}

void Bot::AddPlayer(const Player& p)
{
    players.push_back(p);
    std::cout << p << "\n";
}

void Bot::MinerIncome()
{
    for (auto& p : players)
    {
        p.AddBalance(p.GetIncome() * 5);
        pqxx::work W(conn);
        W.exec_prepared("update_user", p.GetBalance(), p.GetInventory(), p.GetCfWins(), p.GetCfLosses(), p.GetCfProfit(), p.GetUserID());
        W.commit();
    }
}
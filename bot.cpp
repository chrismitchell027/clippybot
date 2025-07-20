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
    {
        sounds.push_back(std::make_pair(it.key(), soundjson[it.key()]["type"]));
        m_mSoundCount[soundjson[it.key()]["author"]]++;
    }
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
        dpp::voiceconn *v = cs.message_event.value().from()->get_voice(cs.guild_id);

        system(std::format("yt-dlp -x --audio-format mp3 https://www.youtube.com/watch?v={} -o yt.mp3", match.str()).c_str());

        //in the same channel
        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
        {
            PlayYoutube(v->voiceclient);
        }
        //not in the same channel
        else if(v != nullptr)
        {
            cs.message_event.value().from()->disconnect_voice(cs.guild_id);
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
        dpp::voiceconn *v = cs.message_event.value().from()->get_voice(cs.guild_id);

        system(std::format("yt-dlp -x --audio-format mp3 \"ytsearch:{}\" -o yt.mp3", match.str()).c_str());

        //in the same channel
        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
        {
            PlayYoutube(v->voiceclient);
        }
        //not in the same channel
        else if(v != nullptr)
        {
            cs.message_event.value().from()->disconnect_voice(cs.guild_id);
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
        dpp::voiceconn *v = cs.message_event.value().from()->get_voice(cs.guild_id);
        if (v && v->voiceclient)
            v->voiceclient->stop_audio();
    }
}

void Bot::CmdSummon(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        dpp::guild *g = dpp::find_guild(cs.guild_id);
        dpp::voiceconn *v = cs.message_event.value().from()->get_voice(cs.guild_id);

        if (v) {
            std::string msg = std::format("active: {}, ready: {}", v->is_active(), v->is_ready());

            this->log(dpp::ll_info, msg);
        }

        if (v != nullptr && g->voice_members[cs.issuer.id].channel_id != v->channel_id)
        {
            cs.message_event.value().from()->disconnect_voice(cs.guild_id);
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
                dpp::voiceconn *v = cs.message_event.value().from()->get_voice(cs.guild_id);

                //in the same channel
                if (v != nullptr && g->voice_members[cs.issuer.id].channel_id == v->channel_id)
                    PlayPCM(v->voiceclient);
                //not in the same channel
                else if(v != nullptr)
                {
                    cs.message_event.value().from()->disconnect_voice(cs.guild_id);
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

dpp::http_request_completion_t Bot::RequestWrapper(std::string url, dpp::http_method m, const nlohmann::json& data)
{
    std::promise<dpp::http_request_completion_t> promise;
    std::future<dpp::http_request_completion_t> future = promise.get_future();

    this->request(url, m, [&promise](const dpp::http_request_completion_t& r)
    {
        promise.set_value(r);
    }, 
    data.dump(), 
    "application/json");

    return future.get();
}

Player Bot::GetPlayer(dpp::snowflake id)
{
    auto response = RequestWrapper(std::format("http://localhost:3000/api/players/{}", (int64_t)id), dpp::m_get);

    if (response.status == 200)
    {
        int soundcount = m_mSoundCount.find(id) != m_mSoundCount.end() ? m_mSoundCount[id] : 0;
        try 
        {
            //member in cache
            auto m = dpp::find_guild_member(SERVER_ID, id);
            return Player(nlohmann::json::parse(response.body), m.get_nickname(), soundcount);
        }
        catch (const dpp::cache_exception& e)
        {
            //member not in cache
            auto msg = std::format("{} not in cache", (int64_t)id);
            this->log(dpp::ll_info, msg);
            *(int*)nullptr = 5;
            return Player();
            //return Player(nlohmann::json::parse(response.body), this->guild_get_member_sync(SERVER_ID, id).get_nickname(), soundcount);
        }
    }
    else
        return Player();
}

std::vector<Player> Bot::GetAllPlayers()
{
    std::vector<Player> players;
    auto response = RequestWrapper("http://localhost:3000/api/players", dpp::m_get);

    if (response.status == 200)
    {
        for (const auto& j : nlohmann::json::parse(response.body))
        {
            int soundcount = m_mSoundCount.find(j["id"]) != m_mSoundCount.end() ? m_mSoundCount[j["id"]] : 0;
            try
            {
                auto m = dpp::find_guild_member(SERVER_ID, j["id"]);
                players.push_back(Player(j, m.get_nickname(), soundcount));
            }
            catch (const dpp::cache_exception& e)
            {
                auto msg = std::format("{} not in cache", (int64_t)j["id"]);
                this->log(dpp::ll_info, msg);
                *(int*)nullptr = 5;
                //players.push_back(Player(j, this->guild_get_member_sync(SERVER_ID, j["id"]).get_nickname(), soundcount));
            }
        }
    }
    else
        throw std::runtime_error("GetAllPlayers error");

    return players;
}

void Bot::CmdRegister(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        nlohmann::json request_data;
        request_data["id"] = cs.issuer.id;

        auto response = RequestWrapper("http://localhost:3000/api/players", dpp::m_post, request_data);
        
        if (response.status == 400)
        {
            cs.message_event.value().reply("You are already registered");
            return;
        }
        else if (response.status != 201)//uhh this should not happen
        {
            *(int*)nullptr = 5;//self destruct
            return;
        }

        cs.message_event.value().reply("You have been registered successfully");
    }
}

void Bot::CmdBalance(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto p = GetPlayer(cs.issuer.id);
        if (p.IsValid())
        {
            cs.message_event.value().reply(std::format("You have {} bebbies", ThousandsFormat(p.GetBalance())));
            return;
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

double ConvertMoney(std::string& m)
{
    auto amt = std::stod(m);

    auto lastChar = tolower(m.back());

    if (lastChar == 'k' || lastChar == 'm' || lastChar == 'b' || lastChar == 't')
    {
        switch (lastChar)
        {
            case 'k':
            {
                amt *= 1000;
                break;
            }
            case 'm':
            {
                amt *= 1000000;
                break;
            }
            case 'b':
            {
                amt *= 1000000000;
                break;
            }
            case 't':
            {
                amt *= 1000000000000;
                break;
            }
        }
    }

    return amt;
}

void Bot::CmdSend(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        if (!(std::holds_alternative<dpp::resolved_user>(param_list[0].second) && std::holds_alternative<std::string>(param_list[1].second) && !std::get<std::string>(param_list[1].second).empty()))
        {
            cs.message_event.value().reply("Usage: $send @someone bebbie_amount");
            return;
        }
        dpp::guild_member m = std::get<dpp::resolved_user>(param_list[0].second).member;

        auto bebbie_amount = std::get<std::string>(param_list[1].second);
        auto amt = ConvertMoney(bebbie_amount);

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

        auto p = GetPlayer(cs.issuer.id);

        if (p.IsValid())
        {
            auto r = GetPlayer(m.user_id);
            if (!r.IsValid())
            {
                cs.message_event.value().reply("Recipient does not have an account");
                return;
            }
            if (p.GetBalance() >= amt)
            {
                p.AddBalance(-amt);
                r.AddBalance(amt);
                cs.message_event.value().reply(std::format("{} has sent {} {} bebbies", p.GetUsername(), r.GetUsername(), ThousandsFormat(amt)));
                return;
            }
            else
            {
                cs.message_event.value().reply("You don't have enough bebbies to send");
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdInventory(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto p = GetPlayer(cs.issuer.id);
        if (p.IsValid())
        {
            dpp::embed inventoryEmbed;
            inventoryEmbed.title = "Inventory";
            inventoryEmbed.color = 0x00DAFF;
            auto miners = Player::GetMiners();

            for (int i = 0; i < p.GetInventory().size(); i++)
                inventoryEmbed.add_field(miners[i].name, std::format("{} Owned", p.GetInventoryItem(i)), true);


            cs.message_event.value().send(dpp::message(cs.channel_id, inventoryEmbed));
            return;
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdShop(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto p = GetPlayer(cs.issuer.id);

        if (p.IsValid())
        {
            dpp::embed inventoryEmbed;
            inventoryEmbed.title = std::format("Bebbies Shop for {}", p.GetUsername());
            inventoryEmbed.color = 0x00DAFF;
            int ItemID = 0;
            auto miners = Player::GetMiners();

            for (int i = 0; i < p.GetInventory().size(); i++)
            {
                inventoryEmbed.add_field(
                std::format("Tier {} [{} Owned]", ItemID + 1, p.GetInventoryItem(i)), 
                std::format("{}\nProduction: {} per second\nCost: {} bebbies", miners[i].name, ThousandsFormat(miners[i].production), ThousandsFormat(p.GetPrice(miners, i))), 
                true);
                ItemID++;
            }


            cs.message_event.value().send(dpp::message(cs.channel_id, inventoryEmbed));
            return;
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
        if (ItemID < 0 || ItemID > Player::GetMiners().size() - 1)
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
            auto p = GetPlayer(cs.issuer.id);
            if (p.IsValid())
            {
                auto miners = Player::GetMiners();
                for (int i = 0; i < amount; i++)
                    totalPrice += p.GetPrice(miners, ItemID, i + 1 + p.GetInventoryItem(ItemID));
                
                if (p.GetBalance() >= totalPrice)
                {
                    for (int i = 0; i < amount; i++)
                        p.BuyItem(miners, ItemID);
                    
                    cs.message_event.value().reply(std::format("{} bought {} {}'s for {}", p.GetUsername(), amount, miners[ItemID].name, ThousandsFormat(totalPrice)));
                    return;
                }
                else
                {
                    cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford {} {}'s for {}", p.GetUsername(), amount, miners[ItemID].name, ThousandsFormat(totalPrice)));
                    return;
                }
            }
        }
        else
        {
            auto p = GetPlayer(cs.issuer.id);
            if (p.IsValid())
            {
                auto miners = Player::GetMiners();
                auto itemPrice = p.GetPrice(miners, ItemID);
                if (p.GetBalance() >= itemPrice)
                {
                    p.BuyItem(miners, ItemID);
                    cs.message_event.value().reply(std::format("{} bought {} for {}", p.GetUsername(), miners[ItemID].name, ThousandsFormat(itemPrice)));
                }
                else
                    cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford a {} for {}", p.GetUsername(), miners[ItemID].name, ThousandsFormat(itemPrice)));
                return;
            }
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdIncome(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto p = GetPlayer(cs.issuer.id);
        if (p.IsValid())
        {
            cs.message_event.value().reply(std::format("{} is currently mining {} bebbies per second", p.GetUsername(), ThousandsFormat(p.GetIncome())));
            return;
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdVault(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        dpp::embed vaultEmbed;
        vaultEmbed.title = "Bebbies Vault";
        vaultEmbed.color = 0x00DAFF;
        int i = 0;
        
        auto players = GetAllPlayers();
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
        auto p = GetPlayer(cs.issuer.id);
        if (p.IsValid())
        {
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (seconds >= p.GetCooldown())
            {
                std::random_device rd;
                std::mt19937 eng(rd());
                long amt;

                if (std::all_of(p.GetInventory().begin(), p.GetInventory().end(), [](int i){ return i == 0; }))//inventory is empty, all 0's
                    amt = std::uniform_int_distribution<>(150, 210)(eng);
                else
                    amt = std::uniform_int_distribution<long>(5.0 * p.GetIncome() * MINE_COOLDOWN / 1000, 10.0 * p.GetIncome() * MINE_COOLDOWN / 1000)(eng);

                p.SetCooldown(seconds);
                p.AddBalance(amt);
                cs.message_event.value().reply(std::format("you mined {} bebbies {}", ThousandsFormat(amt), p.GetUsername()));
            }
            else
            {
                cs.message_event.value().reply(std::format("too soon man, you gotta wait {:.1f} seconds to mine again.", double(p.GetCooldown() - seconds)));
            }
            return;
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

std::unordered_map<dpp::snowflake, double> Bot::GetCoinflips()
{
    auto response = RequestWrapper("http://localhost:3000/api/coinflips", dpp::m_get);
    auto coinflips = nlohmann::json::parse(response.body);

    std::unordered_map<dpp::snowflake, double> cfs;

    for (const auto& [id, amount] : coinflips.items())
        cfs[std::stoll(id)] = amount;

    return cfs;
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

            auto coinflips = GetCoinflips();

            for (const auto& [id, amount] : coinflips)
            {
                cfEmbed.add_field(
                    std::format("{}", GetPlayer(id).GetUsername()),
                    std::format("{} bebbies", ThousandsFormat(amount)),
                    true
                );
            }
            cs.message_event.value().send(dpp::message(cs.channel_id, cfEmbed));
            cs.message_event.value().reply("Usage: $cf bebbies/user");
            return;
        }
        auto value = std::get<std::string>(param_list[0].second);

        if (value == "stats")
        {
            auto p = GetPlayer(cs.issuer.id);
            if (p.IsValid())
            {
                cs.message_event.value().reply(std::format("Coinflip stats for {}: {} wins and {} losses, {:.2f}% win rate, {} bebbies profit", p.GetUsername(), p.GetCfWins(), p.GetCfLosses(),  (double)(p.GetCfWins() * 100) / (p.GetCfWins() + p.GetCfLosses()), ThousandsFormat(p.GetCfProfit())));
                return;
            }
            cs.message_event.value().reply(REGISTER_MSG);
            return;
        }
        else if (value == "cancel")
        {
            auto p = GetPlayer(cs.issuer.id);
            if (p.IsValid())
            {
                auto coinflips = GetCoinflips();

                if (coinflips.find(p.GetUserID()) != coinflips.end())//if player has a coinflip up
                {
                    auto cfValue = coinflips[p.GetUserID()];
                    auto response = RequestWrapper(std::format("http://localhost:3000/api/coinflips/{}", p.GetUserID()), dpp::m_delete);

                    if (response.status == 200)
                        cs.message_event.value().reply(std::format("Coinflip cancelled, {} bebbies refunded", ThousandsFormat(cfValue)));
                    else
                        throw std::runtime_error("Coinflip deletion error");

                    return;
                }
                else
                {
                    cs.message_event.value().reply("No coinflip found");
                    return;
                }
            }
            cs.message_event.value().reply(REGISTER_MSG);
            return;
        }
        else
        {
            if (value.length() == 21 && value.substr(0, 2) == "<@")//if performing the actual cf
            {
                auto p = GetPlayer(cs.issuer.id);
                if (p.IsValid())
                {
                    auto coinflips = GetCoinflips();

                    auto player2 = GetPlayer(std::stoll(value.substr(2, 18)));

                    if (coinflips.find(player2.GetUserID()) != coinflips.end())//if player2 has a coinflip up
                    {
                        if (p.GetBalance() >= coinflips[player2.GetUserID()])
                        {
                            auto response = RequestWrapper(std::format("http://localhost:3000/api/coinflips/{}/{}", player2.GetUserID(), p.GetUserID()), dpp::m_put);
                            bool winner = nlohmann::json::parse(response.body)["winner"];

                            cs.message_event.value().reply(std::format("{} won the coinflip!", winner ? player2.GetUsername() : p.GetUsername()));
                            return;
                        }
                        else
                        {
                            cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford the coinflip", p.GetUsername()));
                            return;
                        }
                    }
                    else
                    {
                        cs.message_event.value().reply("No coinflip found");
                        return;
                    }
                }
            }
            else//if putting the cf up
            {
                if (value == "all")
                {
                    auto p = GetPlayer(cs.issuer.id);
                    if (p.IsValid())
                    {
                        auto coinflips = GetCoinflips();
                        if (coinflips.find(p.GetUserID()) != coinflips.end())
                        {
                            cs.message_event.value().reply(std::format("Cannot have more than one coinflip up", p.GetUsername()));
                            return;
                        }

                        if (p.GetBalance() > 0)
                        {
                            auto response = RequestWrapper(std::format("http://localhost:3000/api/coinflips/{}/{}", p.GetUserID(), p.GetBalance()), dpp::m_post);

                            if (response.status != 200)
                                throw std::runtime_error("Coinflip creation error");

                            cs.message_event.value().reply(std::format("{} put up a coinflip for {}", p.GetUsername(), ThousandsFormat(p.GetBalance())));
                            return;
                        }
                        else
                            cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford the coinflip", p.GetUsername()));

                        return;
                    }
                    cs.message_event.value().reply(REGISTER_MSG);
                    return;
                }

                if (!isdigit(value[0]))
                {
                    cs.message_event.value().reply(std::format("Invalid input"));
                    return;
                }
                //std::remove(value.begin(), value.end(), ',');

                auto amount = ConvertMoney(value);

                if (amount < 0)
                {
                    cs.message_event.value().reply(std::format("Cannot do negative coinflips"));
                    return;
                }

                auto p = GetPlayer(cs.issuer.id);

                if (p.IsValid())
                {
                    auto coinflips = GetCoinflips();

                    if (coinflips.find(p.GetUserID()) != coinflips.end())
                    {
                        cs.message_event.value().reply(std::format("Cannot have more than one coinflip up", p.GetUsername()));
                        return;
                    }
                    if (p.GetBalance() >= amount)
                    {
                        auto response = RequestWrapper(std::format("http://localhost:3000/api/coinflips/{}/{}", p.GetUserID(), amount), dpp::m_post);

                            if (response.status != 200)
                                throw std::runtime_error("Coinflip creation error");

                        cs.message_event.value().reply(std::format("{} put up a coinflip for {}", p.GetUsername(), ThousandsFormat(amount)));
                        return;
                    }
                    else
                        cs.message_event.value().reply(std::format("{} is a broke boy and cannot afford the coinflip", p.GetUsername()));

                    return;
                }

            }
        }

        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdRichest(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        dpp::embed richestEmbed;
        richestEmbed.title = "Richest Players";
        richestEmbed.color = 0x00DAFF;
        int i = 0;
        std::vector<Player> richestPlayers = GetAllPlayers();
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

void Bot::CmdServer(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto miners = Player::GetMiners();
        std::vector serverInv(miners.size(), 0);
        double serverIncome = 0;
        double serverBal = 0;
        auto players = GetAllPlayers();
        int playerCount = players.size();
        for (const auto& p : players)
        {
            serverIncome += p.GetIncome();
            serverBal += p.GetBalance();
            for (int i = 0; i < miners.size(); i++)
                serverInv[i] += p.GetInventoryItem(i);
        }
        dpp::embed invEmbed;
        invEmbed.title = "Server Info";
        invEmbed.color = 0x00DAFF;
        invEmbed.add_field("Income", ThousandsFormat(serverIncome), true);
        invEmbed.add_field("Bebbies", ThousandsFormat(serverBal), true);
        invEmbed.add_field("Players", std::format("{}", playerCount), true);
        for (int i = 0; i < miners.size(); i++)
            invEmbed.add_field(miners[i].name, std::format("[{}]", serverInv[i]), true);

        cs.message_event.value().send(dpp::message(cs.channel_id, invEmbed));
    }
}

void Bot::CmdSoundStats(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        auto p = GetPlayer(cs.issuer.id);
        
        if (p.IsValid())
        {
            cs.message_event.value().reply(std::format("You have {} sounds added to clippy", p.GetSoundCount()));
            return;
        }
        cs.message_event.value().reply(REGISTER_MSG);
    }
}

void Bot::CmdStartLottery(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    /* BOT_SPAM_CHECK
    {
        if (m_bLottery)
        {
            cs.message_event.value().reply("Lottery already running");
            return;
        }

        m_bLottery = true;
        cs.message_event.value().reply("Lottery started - winner will be picked in 15 minutes");
        start_timer([this](dpp::timer t)
        {
            this->m_bLottery = false;

            unsigned int val;
            getrandom(&val, 4, 0);
            val %= this->lottery_entries.size();
            int i = 0;
            for (Player* p : this->lottery_entries)
            {
                if (i == val)
                {
                    p->AddBalance(this->lottery_entries.size() * 100000000000000.0);
                    this->message_create(dpp::message(BOT_SPAM_ID, std::format("{} won {} bebbies in the lottery!", dpp::user::get_mention(p->GetUserID()), ThousandsFormat(this->lottery_entries.size() * 100000000000000.0))));
                    break;
                }
                i++;
            }
            this->lottery_entries.clear();
            this->stop_timer(t);
        }
        , 900);
    } */
    
}

void Bot::CmdEnterLottery(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    /* BOT_SPAM_CHECK
    {
        if (!m_bLottery)
        {
            cs.message_event.value().reply("Lottery hasn't been started yet");
            return;
        }
        for (auto &p : players)
        {
            if (p.GetUserID() == cs.issuer.id)
            {
                if (lottery_entries.find(&p) == lottery_entries.end())
                {
                    if (p.GetBalance() >= 100000000000000)
                    {
                        p.AddBalance(-100000000000000);
                        lottery_entries.insert(&p);
                        cs.message_event.value().reply("You're now entered in the lottery");
                    }
                    else
                        cs.message_event.value().reply("You're broke");
                }
                else
                    cs.message_event.value().reply("You already entered");
                return;
            }
        }

        cs.message_event.value().reply(REGISTER_MSG);
    } */
    
}

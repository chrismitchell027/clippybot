#include "bot.h"

#define BOT_SPAM_CHECK if (cs.channel_id == BOT_SPAM_ID)
#define CLIPPY_ADMIN_CHECK if (cs.channel_id == CLIPPY_ADMIN_ID)

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
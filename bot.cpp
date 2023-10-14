#include "bot.h"
#include <format>
#include <filesystem>

#define BOT_SPAM_CHECK if (cs.channel_id == BOT_SPAM_ID)

/**
 * Reads audio data and puts it into vector,
 * taken from https://dpp.dev/stream-mp3-discord-bot.html
 * @return Audio vector with data
*/
std::vector<uint8_t> Bot::ReadAudioData(const std::string& file_dir)
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

void Bot::CmdClippy(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK 
        cs.message_event.value().reply(CLIPPY_MSG[m_rDistribution(m_eGen)]);
}

void Bot::PlayYoutube(dpp::discord_voice_client *vc)
{
    auto sound_data = ReadAudioData("yt.mp3");

    //Don't need mp3 anymore, it is stored in sound_data now
    std::filesystem::remove("yt.mp3");

    if (vc)
    {
        vc->send_audio_raw((uint16_t*)sound_data.data(), sound_data.size());
    }
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
        std::regex link("([A-Z0-9_]){11}", std::regex_constants::icase);
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
            //m_snoJoinUser = cs.issuer.id;
            g->connect_member_voice(cs.issuer.id, false, true);
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

void Bot::CmdStop(const std::string& cmd, const dpp::parameter_list_t& param_list, dpp::command_source cs)
{
    BOT_SPAM_CHECK
    {
        dpp::voiceconn *v = cs.message_event.value().from->get_voice(cs.guild_id);
        if (v)
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
            //m_snoJoinUser = cs.issuer.id;
            g->connect_member_voice(cs.issuer.id, false, true);
        }
        else if (v == nullptr)
            g->connect_member_voice(cs.issuer.id, false, true);
    }
}
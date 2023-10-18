import nextcord
from nextcord.ext import commands
from PageView import PageView
import asyncio
import json
import os

class SoundCommands(commands.Cog):
    def __init__(self, bot : commands.Bot()):
        self.client = bot
        self.saved_sounds = []
    
    try:
        txt = open("added_sounds.json", "r")#read filenames
        global saved_sounds
        saved_sounds = []
        sound_dict = json.load(txt)
        for sound in sound_dict:
            extension = '.' + sound_dict[sound]["type"]
            saved_sounds.append([sound, extension])#add filename to list
        txt.close()
    except FileNotFoundError:
        pass

    @commands.Cog.listener()
    async def on_message(self, msg):
        author = msg.author

        if author == self.client.user:
            return

        dm = msg.channel.type == nextcord.ChannelType.private

        if not dm:
            await self.client.process_commands(msg)
            return

        guild = self.client.get_guild(402256672028098580)#the boys
        #guild = client.get_guild(968624369683275856) # clippy test server
        member = guild.get_member(author.id)

        if member:
            for r in member.roles:
                if r.id == 501542465623556116:#beaky id     #1139361424217485382: reaction role id for test server
                    if msg.content == "" and msg.attachments and msg.attachments[0].size < 10000000:
                        file_name = msg.attachments[0].filename.lower()
                        for sound in saved_sounds:
                            if sound[0] + sound[1] == file_name:
                                await msg.reply(file_name + " is already taken. Change the name and try again.")
                                return

                        txt = open("added_sounds.json", "r")#add filename to file
                        sound_dict = json.load(txt)
                        txt.close()

                        filetype = file_name[file_name.index('.')+1:]
                        sound_name = file_name[:file_name.index('.')]

                        sound_dict[sound_name] = {"type":filetype, "author":author.id}

                        txt = open("added_sounds.json", "w")
                        json.dump(sound_dict, txt, indent = 4)
                        txt.close()

                        extension = file_name[file_name.index('.'):]
                        saved_sounds.append([file_name[:file_name.index('.')], extension])#add filename to list

                        await msg.attachments[0].save(os.getcwd() + "/sounds/saved_sounds/" + msg.attachments[0].filename.lower())
                        await msg.reply(file_name + " successfully added!")
                    return

    @commands.command()
    async def sounds(self, ctx, sound_name_input: str):
        if ctx.channel.id != 884995892359331850: # bot_spam text channel id
            return
        self.client.stop_sound = False
        file_name = "sounds/"
        found_file = False

        for sound in saved_sounds:
            if sound_name_input == sound[0]:
                file_name += "saved_sounds/" + sound[0] + sound[1]
                found_file = True
                break

        if not found_file:
            await ctx.send(f"{sound_name_input} is not recognized. Type $sounds to see the options.")

        if found_file:
            vc = None
            if ctx.author.id == 228299051517476864 and sound_name_input == "sop" or sound_name_input == "bb" or sound_name_input == "bcs" or sound_name_input == "fuckyou": # jet is not allowed to play loud sounds
                return
            if ctx.author.voice.channel.id != self.client.last_channel or self.client.old_vc == None:
                #await client.get_guild(402256672028098580).change_voice_state(None)
                #vc = nextcord.utils.get(client.voice_clients, guild = ctx.guild)
                if self.client.old_vc != None:
                    await self.client.old_vc.disconnect()
                vc = await ctx.author.voice.channel.connect()
                self.client.last_channel = ctx.author.voice.channel.id
                self.client.old_vc = vc
                await self.client.get_guild(402256672028098580).change_voice_state(channel = ctx.author.voice.channel, self_deaf = True)
            else:
                vc = self.client.old_vc
            vc.play(nextcord.FFmpegPCMAudio(source = file_name))
            while vc.is_playing() and not self.client.stop_sound:
                await asyncio.sleep(.25)
            vc.stop()
            #await vc.disconnect()


    @commands.Cog.listener()
    async def on_sounds_error(self, ctx, error):
        if isinstance(error, commands.MissingRequiredArgument):
            sounds_per_page = 12
            sound_names = [sound[0] for sound in self.saved_sounds]
            pages = partition_sounds_into_pages(sound_names, sounds_per_page)
            view = PageView(pages)
            await view.send_initial_message(ctx)

def setup(client):
    client.add_cog(SoundCommands(client))

def partition_sounds_into_pages(sounds, sounds_per_page):
    # Partitions the list of sounds into pages best on the given number of sounds
    return [sounds[i:i+sounds_per_page] for i in range(0, len(sounds), sounds_per_page)]

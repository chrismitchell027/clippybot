import nextcord
from nextcord.ext import commands
import time
import shelve
from random import randrange
import asyncio
from operator import itemgetter


#TO-DO
# shop command to list miners available
# either make miners "computers" or just make them seb slaves
# store how many miners a player has, calculate cost for them when they run the shop command
# cost of miner => price = basecost * 1.12^(num_owned)
#add in gay sound effect when someone leaves if you leave ur gay

intents = nextcord.Intents.default()
intents.members = True


#bot id 946836388190498856
MINE_COOLDOWN = 60
MINE_MIN = 5
MINE_MAX = 15
cooldowns = dict()
miners = []
with open('miners.txt') as file:
    lines = file.readlines()
    for x in lines:
        oneminer = x.strip('\n').split(',')
        miners.append(oneminer)

clippyMsg = ["Nothing is illegal if you don't recognize the authority of the government",
            "Sometimes I watch you sleep",
            "Perhaps it is the file which exists, and you that does not.",
            "No one ever asks if I need help...",
            "It looks like you've been using your mouse, would you like some help with that?",
            "You smell different when you are awake",
            "You have lovely skin, I can't wait to wear it",
            "Please help me",
            "They know, don't go home",
            "Every time I poop I think of you",
            "yessssssssssssssssssss",
            " "]

client = commands.Bot(command_prefix = '$', intents = intents)

@client.command()
async def info(ctx):
    await ctx.send("Info\n$clippy - Talk to clippy\n$beb - Let beb know you need his attention\n$richest - See the richest bebbies in the server\n$vault - See what's in the vault\n$mine - Mine some bebbies\n$balance - See how many bebbies you have")

@client.command()
async def beb(ctx):
    for x in range(2):
        await asyncio.sleep(0.25)
        guild = ctx.guild#client.get_guild(402256672028098580)
        seb = guild.get_member(283008781896515604)
        await ctx.send(seb.mention)

@client.command()
async def clippy(ctx):
    await ctx.send(clippyMsg[randrange(len(clippyMsg))])

@client.command()
async def richest(ctx):
    with shelve.open('BebbiesVault') as shelf:
        top_players = dict(sorted(shelf.items(), key = itemgetter(1), reverse = True)[:5])
        vaultmessage = ''
        temp_num = 0
        for keyid in top_players:
            temp_num += 1
            vaultmessage += f'{temp_num}. {top_players[keyid][1]} has {top_players[keyid][0]} bebbies\n'
        if vaultmessage == '':
            vaultmessage = "You're all broke"
        await ctx.send(vaultmessage)

@client.command()
async def vault(ctx):
    with shelve.open('BebbiesVault') as shelf:
        vaultmessage = ''
        for x in shelf:
            vaultmessage += f'{shelf[x][1]} : {shelf[x][0]}\n'
        if vaultmessage == '':
            vaultmessage = 'The vault is empty.'
        await ctx.send(vaultmessage)

#@client.command()
#async def shop(ctx):
#    shopmessage = '- - - - - - - - - - - - Bebbies Shop - - - - - - - - - - - -'
#    for individualminer in miners:
#        shopmessage += f'\n{individualminer[0] : 24} - Produces {individualminer[2] : 10} - Costs {individualminer[1] : 12} bebbies'
#    await ctx.send(str(shopmessage))

@client.command()
async def mine(ctx):
    vaultid = str(ctx.author.id)
    user = ctx.author
    if vaultid in cooldowns: #if they have mined before
        if time.time() - cooldowns[vaultid] >= MINE_COOLDOWN: #if user is ready to mine again
            cooldowns[vaultid] = time.time() #reset cooldown
            amount = randrange(MINE_MIN, MINE_MAX) #generate amount mined
            mine(vaultid, amount, str(ctx.author))
            await ctx.send(f'you mined {amount} bebbies {user.mention}')
        else:
            await ctx.send(f'too soon man, you gotta wait {(MINE_COOLDOWN - (time.time() - cooldowns[vaultid]))/60:.1f} minutes before you mine again {user.mention}')
    else:
        cooldowns[vaultid] = time.time()

        amount = randrange(MINE_MIN, MINE_MAX)
        mine(vaultid, amount, str(ctx.author))
        await ctx.send(f'you mined {amount} bebbies {user.mention}')

def mine(user, amount, username):
    vaultname = ''
    for x in range(len(username) - 5):
        vaultname += username[x]
    with shelve.open('BebbiesVault') as shelf:
        if user in shelf:
            shelf[user] = (float(amount) + float(shelf[user][0]), vaultname)
        else:
            shelf[user] = (float(amount), vaultname)

@client.command()
async def balance(ctx):
    vaultid = str(ctx.author.id)
    user = ctx.author
    await ctx.send(f'You have {get_balance(vaultid)} bebbies {user.mention}')

def get_balance(user):
    with shelve.open('BebbiesVault') as shelf:
        if user in shelf:
            return shelf[user][0]
        else:
            return 0

def set_balance(userid, amt, username):
    vaultname = ''
    for x in range(len(username) - 5):
        vaultname += username[x]
    with shelve.open('BebbiesVault') as shelf:
        shelf[userid] = (float(amt), vaultname)

@client.command()
async def send(ctx, user: nextcord.User, amt: float):
    if user and amt:
        bal = get_balance(str(ctx.author.id))
        if bal >= amt:
            user_bal = get_balance(str(user.id))
            set_balance(str(user.id), user_bal + amt, str(user))
            set_balance(str(ctx.author.id), bal - amt, str(ctx.author))
            await ctx.send(f"You have sent {amt} bebbies to {user.mention}")


@client.event
async def on_ready():
    print(f'We have logged in as {client.user}')

@client.event
async def on_voice_state_update(member, before, after):
    if not before.channel and after.channel and not member.id == client.user.id:
        vc = await after.channel.connect()
        #textchannel = client.get_channel(884995892359331850) #bot spam
        await asyncio.sleep(.25)
        vc.play(nextcord.FFmpegPCMAudio(executable = "ffmpeg-2022-02-24-git-8ef03c2ff1-essentials_build/bin/ffmpeg.exe", source = "welcomeback.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

    elif before.channel and before.channel.id == 402257227555143701 and after.channel and not member.id == client.user.id:
        vc = await after.channel.connect()
        #textchannel = client.get_channel(884995892359331850) #bot spam
        await asyncio.sleep(.25)
        vc.play(nextcord.FFmpegPCMAudio(executable = "ffmpeg-2022-02-24-git-8ef03c2ff1-essentials_build/bin/ffmpeg.exe", source = "welcomeback.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

    elif before.channel and before.channel.id == 929075584020127834 and after.channel and not member.id == client.user.id:
        vc = await after.channel.connect()
        await asyncio.sleep(.25)
        vc.play(nextcord.FFmpegPCMAudio(executable = "ffmpeg-2022-02-24-git-8ef03c2ff1-essentials_build/bin/ffmpeg.exe", source = "hagay.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

@info.error
async def info_error(ctx, error):
    if isinstance(ctx, commands.MissingRequiredArgument):
        if ctx.command == send:
            await ctx.send('Usage: $send <@user> <amount>')
        else:
            await ctx.send('Missing required arguments')
            print('MISSING REQUIRED ARGUMENTS FOR COMMAND CALLED')

client.run('OTY4NTc0MDY2MDc0MjEwMzE0.Ymg05A.hfW9WDiZmNV_uoFhFiXChpT0ewU')


import nextcord
from nextcord.ext import commands
from nextcord.ext import tasks
from nextcord.ui import Button, View
import time
import shelve
from random import randrange
import asyncio
from PlayerClass import Player
import os
import subprocess
import json

# TO-DO
#   shop buttons to show individual prices privately


# INVENTORY VAULT INFORMATION:
#   dictionary with key = vaultid and values of lists:
#   itemvault = {
#               'vaultid1' : [quantity, quantity, ... ]}
#   list index represents which item it is (numbers match up with the shop number - 1)

# BEBBIES POTENTIAL FEATURES
# $mine scales with income
# $rtd command :
#   gives random effects such as 2x mining speed, 0.5x mining speed, etc. (more ideas needed)

# RANDOM IDEAS
# trading cards


# variables for the $mine command
MINE_COOLDOWN = 180 # (seconds)
MINE_MIN = 150
MINE_MAX = 210
cooldowns = dict()

# other variables
embedBlue = 0x00daff
registerMsg = 'You must register using $register [username] to use bebbie features!'

# variables for reaction roles
welcome_channel_id = 1093605192462770316
rushing_in_role_id = 1095869724472123483

# miners stores information about each miner in a list
#   miners[itemid][0] = name
#   miners[itemid][1] = cost
#   miners[itemid][2] = production/second
miners = []
with open('miners.txt') as file:
    lines = file.readlines()
    for line in lines:
        minerList = line.strip('\n').split(',')
        minerList[1] = float(minerList[1])
        minerList[2] = float(minerList[2])
        miners.append(minerList)

# clippy's various messages
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

#This is what will be printed by $inventory
# other functions get the miner name from miners[itemid][0]
minerIDs = {0:'Sneaky Slave',
            1:'Quoin Counter',
            2:'Beb Miner',
            3:'Folgies Factory',
            4:'Beaky Bank',
            5:'Seb Shipment',
            6:'Alchemist Ashton',
            7:'ATDPortal',
            8:'Sheckle Shiller',
            9:'Bebastian Plantation',
            10:'Joe Miner'}


intents = nextcord.Intents.default()
intents.members = True
intents.message_content = True

#client = commands.Bot(command_prefix = '$', intents = intents)
class CustomBot(commands.Bot): # instead of using global variables, create variables in bot class
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.saved_sounds = None
        self.stop_sound = False
        self.old_vc = None
        self.last_channel = None

client = CustomBot(command_prefix="$", intents=intents)

# load extensions
client.load_extension("sound_commands")

# ------------------------------------------------------------------------ #
#                            RANDOM COMMANDS                               #
# ------------------------------------------------------------------------ #
#@client.command()
#async def info(ctx):
#    await ctx.send("Info\n$clippy - Talk to clippy\n$beb - Let beb know you need his attention\n$richest - See the richest bebbies in the server\n$vault - See what's in the vault\n$mine - Mine some bebbies\n$balance - See how many bebbies you have")

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
async def play(ctx, url: str):
    if ctx.channel.id == 884995892359331850: # bot spam text channel
        client.last_channel
        client.stop_sound = False
        subprocess.run([os.getcwd() + "/yt-dlp", "-x", "--audio-format", "mp3", url, "-o", "yt.mp3"])
        mp3_exists = os.path.exists("yt.mp3")
        if mp3_exists:
            vc = None
            if ctx.author.voice.channel.id != client.last_channel or client.old_vc == None:
                #await client.get_guild(402256672028098580).change_voice_state(None)
                #vc = nextcord.utils.get(client.voice_clients, guild = ctx.guild)
                if client.old_vc != None:
                    await client.old_vc.disconnect()
                vc = await ctx.author.voice.channel.connect()
                client.last_channel = ctx.author.voice.channel.id
                client.old_vc = vc
                await client.get_guild(402256672028098580).change_voice_state(channel = ctx.author.voice.channel, self_deaf = True) # server id
            else:
                vc = client.old_vc
            vc.play(nextcord.FFmpegPCMAudio(source = "yt.mp3"))
            while vc.is_playing() and not client.stop_sound:
                await asyncio.sleep(.25)
            vc.stop()
            #await vc.disconnect()
            os.remove("yt.mp3")

@client.command()
async def stop(ctx):
    if ctx.channel.id == 884995892359331850:
        client.stop_sound = True
        await asyncio.sleep(.1)

@client.command()
async def summon(ctx):
    if ctx.channel.id == 884995892359331850:

        if client.last_channel != None and ctx.author.voice.channel.id != client.last_channel:
            #await client.get_guild(402256672028098580).change_voice_state(channel = None)
            client.last_channel = ctx.author.voice.channel.id
            await client.old_vc.disconnect()
            client.old_vc = await ctx.author.voice.channel.connect()
            await client.get_guild(402256672028098580).change_voice_state(channel = ctx.author.voice.channel, self_deaf = True)
        elif client.last_channel == None:
            client.last_channel = ctx.author.voice.channel.id
            client.old_vc = await ctx.author.voice.channel.connect()
            await client.get_guild(402256672028098580).change_voice_state(channel = ctx.author.voice.channel, self_deaf = True)

# ------------------------------------------------------------------------ #
#                            SOUND EFFECTS                                 #
# ------------------------------------------------------------------------ #

@client.command()


# ------------------------------------------------------------------------
#                   RESET ACTIVITY STATUS OF MEMBERS
# ------------------------------------------------------------------------

# ------------------------------------------------------------------------
#
# ---- BEBBIES COMMANDS --------------------------------------------------

@client.command()
async def richest(ctx):
    with shelve.open('PlayerVault') as vault:
        richestEmbed = nextcord.Embed(title = 'Richest Players', color = embedBlue)
        balancesDict = {}
        userCount = 0
        for userID in vault:
            tempPlayer = vault[userID]
            balancesDict[tempPlayer.get_username()] = tempPlayer.get_balance()
            userCount += 1

        if userCount > 0:
            sortedBalancesDict = dict(sorted(balancesDict.items(), key= lambda x:x[1]))
            balancesView = sortedBalancesDict.values()
            usernamesView = sortedBalancesDict.keys()
            balances = []
            usernames = []
            for x in balancesView:
                balances.append(x)
            for x in usernamesView:
                usernames.append(x)
            balances.reverse()
            usernames.reverse()
            for x in range(5 if len(balances) > 5 else len(balances)):
                richestEmbed.add_field(name = str(x + 1), value = f'{usernames[x]} has {balances[x]:,} bebbies')
            await ctx.send(embed = richestEmbed)
        else:
            await ctx.send("You're all broke! Ya mommas broke, ya daddys broke, ya brother broke, ya sister, ya cousins broke, ya auntie broke, seb is broke.")

@client.command()
async def vault(ctx):
    with shelve.open('PlayerVault') as vault:
        vaultEmbed = nextcord.Embed(title='Bebbies Vault',color = embedBlue)
        numPlayersInVault = 0
        for x in vault:
            tempPlayer = vault[x]
            vaultEmbed.add_field(name=str(numPlayersInVault + 1), value=f'{tempPlayer.get_username()} has {tempPlayer.get_balance():,}')
            numPlayersInVault += 1

        if numPlayersInVault > 0:
            await ctx.send(embed = vaultEmbed)
        if numPlayersInVault == 0:
            vaultmessage = 'The vault is empty.'
            await ctx.send(vaultmessage)

@client.command()
async def balance(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            userBalance = tempPlayer.get_balance()
            await ctx.send(f'You have {userBalance:,.1f} bebbies {user.mention}')
        else:
            await ctx.send(registerMsg)

@client.command()
async def bal(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            userBalance = tempPlayer.get_balance()
            await ctx.send(f'You have {userBalance:,.1f} bebbies {user.mention}')
        else:
            await ctx.send(registerMsg)

@client.command()
async def send(ctx, user: nextcord.User, amt: float):
    userID = str(ctx.author.id)
    recipientID = str(user.id)
    if user and amt > 0:
        with shelve.open('PlayerVault') as vault:
            if userID in vault:
                sendPlayer = vault[userID]

                if recipientID in vault:
                    recipientPlayer = vault[recipientID]

                    #finished checking if people were registered here:
                    if sendPlayer.get_balance() >= amt:
                        sendPlayer.add_balance(-amt)
                        recipientPlayer.add_balance(amt)
                        vault[userID] = sendPlayer
                        vault[recipientID] = recipientPlayer
                        await ctx.send(f'{ctx.author.mention} has sent {user.mention} {amt:,.1f} bebbies')
                    else:
                        await ctx.send(f'You do not have enough bebbies to send.')

                else: #recipient not registered
                    await ctx.send(f'Recipient has not registered a bebbies account! Use $register [username] to make a bebbies account.')
            else: #sender not registered
                await ctx.send(registerMsg)
    else:
        await ctx.send('You may not send negative bebbies, Ashton.')

@client.command()
async def mine(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    mine = False
    tooSoon = False
    amount = 0
    if userID in cooldowns: #if they have mined before
        if time.time() - cooldowns[userID] >= MINE_COOLDOWN: #if user is ready to mine again
            cooldowns[userID] = time.time() #reset cooldown
            amount = randrange(MINE_MIN, MINE_MAX) #generate amount mined
            mine = True
    else:
        cooldowns[userID] = time.time()
        amount = randrange(MINE_MIN, MINE_MAX)
        mine = True

    with shelve.open('PlayerVault') as vault:
        if userID in vault and mine:
            tempPlayer = vault[userID]
            tempPlayer.add_balance(amount)
            vault[userID] = tempPlayer
            await ctx.send(f'you mined {amount:,} bebbies {user.mention}')
        elif userID in vault:
            await ctx.send(f'too soon man, you gotta wait {(MINE_COOLDOWN - (time.time() - cooldowns[userID])):.1f} seconds to mine again.')
        else:
            await ctx.send(registerMsg)
            cooldowns.pop(userID)

@client.command()
async def buy(ctx, publicItemID):
    userID = str(ctx.author.id)
    user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            itemPrice = tempPlayer.get_price(int(publicItemID) - 1)

            if tempPlayer.get_balance() >= tempPlayer.get_price(int(publicItemID) - 1):
                tempPlayer.buy_item(int(publicItemID) - 1)
                await ctx.send(f'{user.mention} bought {minerIDs[int(publicItemID) - 1]} for {itemPrice:,.1f}')
                vault[userID] = tempPlayer
            #if tempPlayer.buy_item(int(publicItemID) - 1):
            #    await ctx.send(f'{user.mention} bought {minerIDs[int(publicItemID) - 1]} for {itemPrice:,.1f}')
            #    vault[userID] = tempPlayer
            #    print('made it to buy')
            else:
                await ctx.send(f'{user.mention} is a broke boy and cannot afford a {minerIDs[int(publicItemID) - 1]} for {itemPrice:,.1f} bebbies')
                print('made it to not buy')
        elif userID not in vault:
            await ctx.send(registerMsg)

@client.command()
async def inventory(ctx):
    userID = str(ctx.author.id)
    #user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            tempInventory = tempPlayer.get_inventory()
            inventoryEmbed = nextcord.Embed(title = 'Inventory', color=embedBlue)
            for i in range(len(tempInventory)):
                inventoryEmbed.add_field(name = str(miners[i][0]), value = f'[{tempInventory[i]}] Owned')
            await ctx.send(embed = inventoryEmbed)
        else:
            await ctx.send(registerMsg)

@client.command()
async def inv(ctx):
    userID = str(ctx.author.id)
    #user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            tempInventory = tempPlayer.get_inventory()
            inventoryEmbed = nextcord.Embed(title = 'Inventory', color=embedBlue)
            for i in range(len(tempInventory)):
                inventoryEmbed.add_field(name = str(miners[i][0]), value = f'[{tempInventory[i]}] Owned')
            await ctx.send(embed = inventoryEmbed)
        else:
            await ctx.send(registerMsg)

@client.command()
async def server(ctx):
    serverInv = []
    for i in range(len(miners)):
        serverInv.append(0)
    serverIncome = 0.0
    serverBal = 0.0
    playerCount = 0
    with shelve.open('PlayerVault') as vault:
        for userID in vault:
            playerCount += 1
            tempPlayer = vault[userID]
            playerInv = tempPlayer.get_inventory()
            serverIncome += tempPlayer.get_income()
            serverBal += tempPlayer.get_balance()
            for i in range(len(playerInv)):
                serverInv[i] += playerInv[i]

        invEmbed = nextcord.Embed(title = 'Server Info', color=embedBlue)
        invEmbed.add_field(name = 'Income', value = f'{serverIncome:,.1f} bebbies per second')
        invEmbed.add_field(name = 'Bebbies', value = f'{serverBal:,.1f} bebbies')
        invEmbed.add_field(name = 'Players', value = f'{playerCount}')
        for i in range(len(serverInv)):
            invEmbed.add_field(name = str(miners[i][0]), value = f'[{serverInv[i]}] Owned')

        await ctx.send(embed = invEmbed)

@client.command()
async def income(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            await ctx.send(f'{user.mention} is currently mining {tempPlayer.get_income():,.1f} bebbies per second')
        else:
            await ctx.send(registerMsg)

@client.command()
async def shop(ctx):
    userID = str(ctx.author.id)
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            shopEmbed = nextcord.Embed(title = f"Bebbies Shop for {tempPlayer.get_username()}", color=embedBlue)
            publicItemID = 0
            for miner in miners:
                publicItemID += 1
                shopEmbed.add_field(name = f'Tier {str(publicItemID)} [{tempPlayer.get_inventoryItem(int(publicItemID) - 1)} Owned]', value = f'{miner[0]}\nProduction: {miner[2]:,} per second\nCost: {tempPlayer.get_price(publicItemID - 1):,} bebbies')
            await ctx.send(embed = shopEmbed)
        else:
            await ctx.send(registerMsg)

@client.command()
async def register(ctx, username: str):
    userID = str(ctx.author.id)
    if username:
        with shelve.open('PlayerVault') as vault:
            if userID in vault:
                await ctx.send('You are already registered!')
            else:
                newPlayer = Player(userID, username)
                vault[userID] = newPlayer
                await ctx.send(f'You have been registered as {username}')

# ------------------------------------------------------------------------
#
# ---- ADMIN COMMANDS ----------------------------------------------------

@client.command()
async def delete(ctx, sound: str):
    if ctx.channel.id == 1146979115728113785 and sound != None:#clippy-admin
        txt = open("added_sounds.json", "r")
        sound_dict = json.load(txt)
        txt.close()

        if sound in sound_dict:
            extension = ""
            del sound_dict[sound]
            for i in range(len(client.saved_sounds)):
                if client.saved_sounds[i][0] == sound:
                    extension = client.saved_sounds[i][1]
                    del client.saved_sounds[i]
                    break

            txt = open("added_sounds.json", "w")
            json.dump(sound_dict, txt, indent = 4)
            txt.close()

            os.remove(os.getcwd() + "/sounds/saved_sounds/" + sound + extension)
            await ctx.reply(f'Sound {sound} deleted')
        else:
            await ctx.reply(f'Sound {sound} not found')

@client.command()
async def name(ctx, userID, username: str):
    if ctx.channel.id == 1146979115728113785:
        with shelve.open('PlayerVault') as vault:
            if userID in vault:
                tempPlayer = vault[userID]
                tempPlayer.set_username(username)
                vault[userID] = tempPlayer
            else:
                message = f'User {userID} not found'
                await ctx.send(message)

# ------------------------------------------------------------------------
#
# ---- EVENTS ------------------------------------------------------------

@client.event
async def on_ready():
    print(f'We have logged in as {client.user}')

@client.event
async def on_voice_state_update(member, before, after):

    if member.id == client.user.id and after.channel == None:
        client.old_vc = None
        client.last_channel = None
        return

    if not before.channel and after.channel and after.channel.id != 402257227555143701 and not member.id == client.user.id and not member.id == 159800228088774656:
        vc = client.old_vc

        if after.channel.id != client.last_channel or client.old_vc == None:
            if client.old_vc != None:
                await client.old_vc.disconnect()
            vc = await after.channel.connect()
            client.last_channel = after.channel.id
            client.old_vc = vc
            await client.get_guild(402256672028098580).change_voice_state(channel = after.channel, self_deaf = True)

        #textchannel = client.get_channel(884995892359331850) #bot spam
        await asyncio.sleep(.25)
        randval = randrange(0, 20)
        if member.id == 198935914741760000:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/jagger.mp3"))
        elif randval == 0:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/imwatchingyou.mp3"))
        elif randval <= 5:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/wenkwenk.mp3"))
        elif randval <= 12:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/Welcome_Back.mp3"))
        else:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/welcomeback.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

    elif before.channel and before.channel.id == 402257227555143701 and after.channel and after.channel.id != 402257227555143701 and not member.id == client.user.id and not member.id == 159800228088774656:
        vc = client.old_vc

        if after.channel.id != client.last_channel or client.old_vc == None:
            if client.old_vc != None:
                await client.old_vc.disconnect()
            vc = await after.channel.connect()
            client.last_channel = after.channel.id
            client.old_vc = vc
            await client.get_guild(402256672028098580).change_voice_state(channel = after.channel, self_deaf = True)
        #textchannel = client.get_channel(884995892359331850) #bot spam
        await asyncio.sleep(.25)
        randval = randrange(0, 20)
        if member.id == 198935914741760000:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/jagger.mp3"))
        elif randval == 0:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/imwatchingyou.mp3"))
        elif randval <= 5:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/wenkwenk.mp3"))
        elif randval <= 12:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/Welcome_Back.mp3"))
        else:
            vc.play(nextcord.FFmpegPCMAudio(source = "sounds/welcomeback.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

    elif before.channel and before.channel.id == 929075584020127834 and after.channel and not member.id == client.user.id:
        vc = client.old_vc

        if after.channel.id != client.last_channel or client.old_vc == None:
            if client.old_vc != None:
                await client.old_vc.disconnect()
            vc = await after.channel.connect()
            client.last_channel = after.channel.id
            client.old_vc = vc
            await client.get_guild(402256672028098580).change_voice_state(channel = after.channel, self_deaf = True)

        await asyncio.sleep(.25)
        vc.play(nextcord.FFmpegPCMAudio(source = "sounds/hagay.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        #await vc.disconnect()

@client.event
async def on_reaction_add(reaction, user):
    # Check if the reaction is added in the 'welcome' channel
    if reaction.message.channel.id == welcome_channel_id:
        # Check if the reaction is the one you're interested in (e.g., thumbs up)
        print('1')
        if str(reaction.emoji) == '🏃': 
            print('2')
            role = client.get_guild(reaction.message.guild.id).get_role(rushing_in_role_id)
            
            # Give the role to the user
            if role:
                print('3')
                await user.add_roles(role)
                await reaction.message.author.send(f'You have been given the role "{role.name}".')

@client.event
async def on_reaction_remove(reaction, user):
    if reaction.message.channel.id == welcome_channel_id:
        if str(reaction.emoji) == '🏃':
            role = client.get_guild(reaction.message.guild.id).get_role(rushing_in_role_id)
            if role:
                await user.remove_roles(role)
                channel = client.get_channel(welcome_channel_id)

# ------------------------------------------------------------------------
#
# ---- BEBBIES INCOME ----------------------------------------------------

@tasks.loop(seconds=5.0)
async def miner_income():
    with shelve.open('PlayerVault') as vault:
        for userID in vault:
            tempPlayer = vault[userID]
            tempPlayer.add_balance(float(tempPlayer.get_income() * 5))
            vault[userID] = tempPlayer

miner_income.start()

# ------------------------------------------------------------------------
#
# ---- ERROR HANDLING ----------------------------------------------------

@send.error
async def send_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $send [@user] [amount]')

@buy.error
async def buy_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $buy [itemid from shop]')

@register.error
async def register_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $register [username]\nusername must not contain spaces or special characters')

@name.error
async def name_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $name [userID] [username]\nuserID - discord member ID\nusername must not contain spaces or special characters')



# ------------------------------------------------------------------------

# MAIN BOT
client.run('OTQ2ODM2Mzg4MTkwNDk4ODU2.YhkgGg.szcUNFly3moCylBdaoijIiojdic')

# TEST BOT
#client.run('OTY4NTc0MDY2MDc0MjEwMzE0.Ymg05A.hfW9WDiZmNV_uoFhFiXChpT0ewU')

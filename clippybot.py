import nextcord
from nextcord.ext import commands
from nextcord.ext import tasks
import time
import shelve
from random import randrange
import asyncio
from PlayerClass import Player
import os
import subprocess
import json

# TO-DO
#   either make miners "computers" or just make them seb slaves
#   store how many miners a player has, calculate cost for them when they run the shop command
#   cost of miner => price = basecost * 1.12^(num_owned)
#   shop buttons to show individual prices privately


# INVENTORY VAULT INFORMATION:
#   dictionary with key = vaultid and values of lists:
#   itemvault = {
#               'vaultid1' : [quantity, quantity, ... ]}
#   list index represents which item it is (numbers match up with the shop number - 1)


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

saved_sounds = []

intents = nextcord.Intents.default()
intents.members = True
intents.message_content = True

client = commands.Bot(command_prefix = '$', intents = intents)

# ---- RANDOM COMMANDS ---------------------------------------------------

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
    subprocess.run([os.getcwd() + "/yt-dlp", "-x", "--audio-format", "mp3", url, "-o", "yt.mp3"])
    mp3_exists = os.path.exists("yt.mp3")
    if mp3_exists:
        vc = await ctx.author.voice.channel.connect()
        vc.play(nextcord.FFmpegPCMAudio(source = "yt.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()
        os.remove("yt.mp3")



# ------------------------------------------------------------------------
#                            SOUND EFFECTS
# ------------------------------------------------------------------------

@client.command()
async def sounds(ctx, sound: str):
    file_name = "sounds/"
    flag = True
    match sound:
        case "amogus":
            file_name += "amogus.m4a"
        case "augh":
            file_name += "augh.mp3"
        case "pbnj":
            file_name += "pbnj.mp3"
        case "ayoterry":
            file_name += "ayoterry.mp3"
        case "bababooey":
            file_name += "bababooey.3gp"
        case "dog":
            file_name += "dog.3gp"
        case "error":
            file_name += "error.3gp"
        case "marioo":
            file_name += "marioo.3gp"
        case "taco":
            file_name += "taco.3gp"
        case "usb":
            file_name += "usb.3gp"
        case "wenkwenk":
            file_name += "wenkwenk.mp3"
        case "aight":
            file_name += "aight.mp3"
        case _:
            found_file = False
            for sounds in saved_sounds:
                if sound == sounds[0]:
                    file_name += "saved_sounds/" + sounds[0] + sounds[1]
                    found_file = True
                    break
            if not found_file:
                await ctx.send(f"{sound} is not recognized. Type $sounds to see the options.")
                flag = False
    if flag:
        vc = await ctx.author.voice.channel.connect()
        vc.play(nextcord.FFmpegPCMAudio(source = file_name))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()

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
            for i in range(len(saved_sounds)):
                if saved_sounds[i][0] == sound:
                    extension = saved_sounds[i][1]
                    del saved_sounds[i]
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

@client.event
async def on_voice_state_update(member, before, after):
    if not before.channel and after.channel and after.channel.id != 402257227555143701 and not member.id == client.user.id and not member.id == 159800228088774656:
        vc = await after.channel.connect()
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
        vc = await after.channel.connect()
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
        vc = await after.channel.connect()
        await asyncio.sleep(.25)
        vc.play(nextcord.FFmpegPCMAudio(source = "sounds/hagay.mp3"))
        while vc.is_playing():
            await asyncio.sleep(.25)
        vc.stop()
        await vc.disconnect()



@client.event
async def on_message(msg):
    author = msg.author

    if author == client.user:
        return

    dm = msg.channel.type == nextcord.ChannelType.private

    if not dm:
        await client.process_commands(msg)
        return

    guild = client.get_guild(402256672028098580)#the boys
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

@sounds.error
async def sounds_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        sounds_page_number = 1 #current number of the embed being created
        sounds_added_count = 0 #how many sounds are on a given embed
        soundsEmbed = nextcord.Embed(title = f'Sounds Page {sounds_page_number}', color = embedBlue) #sets up first embed

        #adds original sounds and updates sounds_added_count
        soundsEmbed.add_field(name = 'amogus', value = '')
        soundsEmbed.add_field(name = 'augh', value = '')
        soundsEmbed.add_field(name = 'pbnj', value = '')
        soundsEmbed.add_field(name = 'ayoterry', value = '')
        soundsEmbed.add_field(name = 'bababooey', value = '')
        soundsEmbed.add_field(name = 'dog', value = '')
        soundsEmbed.add_field(name = 'error', value = '')
        soundsEmbed.add_field(name = 'marioo', value = '')
        soundsEmbed.add_field(name = 'taco', value = '')
        soundsEmbed.add_field(name = 'usb', value = '')
        soundsEmbed.add_field(name = 'wenkwenk', value = '')
        soundsEmbed.add_field(name = 'aight', value = '')
        sounds_added_count += 12
        
        #iterates through all sounds in the saved sounds file
        for s in saved_sounds:
            soundsEmbed.add_field(name = s[0], value = '') #adds sound to embed
            sounds_added_count += 1 #update sound count
            if sounds_added_count == 25: #once 25 sounds are added, print the embed and setup the next embed
                await ctx.send(embed=soundsEmbed)
                sounds_page_number += 1
                soundsEmbed = nextcord.Embed(title = f'Sounds Page {sounds_page_number}', color = embedBlue)
                sounds_added_count = 0

        # at this point there are no more sounds in the saved_sounds file to add
        # if the current embed has any sounds on it, print (used to prevent an empty embed from being printed because of a sound count that is a multiple of 25)
        if sounds_added_count > 0:
            await ctx.send(embed=soundsEmbed)

# ------------------------------------------------------------------------

# MAIN BOT
client.run('OTQ2ODM2Mzg4MTkwNDk4ODU2.YhkgGg.szcUNFly3moCylBdaoijIiojdic')

# TEST BOT
#client.run('OTY4NTc0MDY2MDc0MjEwMzE0.Ymg05A.hfW9WDiZmNV_uoFhFiXChpT0ewU')

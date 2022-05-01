from operator import itemgetter
import nextcord
from nextcord.ext import commands
from nextcord.ext import tasks
import time
import shelve
from random import randrange
import asyncio
from PlayerClass import Player

# TO-DO
#   shop command to list miners available
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
MINE_COOLDOWN = 3600 # (seconds)
MINE_MIN = 500
MINE_MAX = 700
cooldowns = dict() 

# other variables
embedBlue = 0x00daff


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

client = commands.Bot(command_prefix = '$', intents = intents)

# NEEDS TO BE UPDATED FOR NEW COMMANDS, POSSIBLY FIND WAY TO READ FROM TEXT FILE
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
    with shelve.open('PlayersVault') as vault:
        top_players = dict()
        for player in vault:
            tempPlayer = player
            top_players[player.get_userID] = player.get_balance()
        top_players = sorted(top_players, reverse = True)[:5]
        richestEmbed = nextcord.Embed(title='Richest Players', color=embedBlue)
        numRichPlayers = 0
        for player in top_players:
            richestEmbed.add_field(name=str(numRichPlayers + 1), value=f'{top_players[0]}')
            top_players.pop(0)
            numRichPlayers += 1
        else:
            await ctx.send(embed = richestEmbed)
        if numRichPlayers == 0:
            vaultmessage = "You're all broke"
            await ctx.send(vaultmessage)

@client.command()
async def vault(ctx):
    with shelve.open('PlayerVault') as vault:
        vaultEmbed = nextcord.Embed(title='Bebbies Vault',color=embedBlue)
        numPlayersInVault = 0
        for x in vault:
            tempPlayer = vault[x]
            vaultEmbed.add_field(name=str(numPlayersInVault + 1), value=f'{tempPlayer.get_username()} has {tempPlayer.get_balance()}')
            numPlayersInVault += 1
        else:
            await ctx.send(embed = vaultEmbed)
        if numPlayersInVault == 0:
            vaultmessage = 'The vault is empty.'
            await ctx.send(vaultmessage)

def get_cost(user, item):
    basePrice = int(miners[item][1])
    #get amount owned by user
    #calculate and return custom cost
    return basePrice

@client.command()
async def shop(ctx):
    userID = str(ctx.author.id)
    shopembed = nextcord.Embed(title = "Bebbies Shop", color=embedBlue)
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
        else:
            generate_player(userID, str(ctx.author))
            tempPlayer = vault[userID]
        
        publicItemID = 0
        for miner in miners:
            publicItemID += 1
            shopembed.add_field(name = 'Tier ' + str(publicItemID), value = f'{miner[0]}\nProduction: {miner[2]:,} per second\nCost: {tempPlayer.get_price(publicItemID - 1):,} bebbies')
    
    await ctx.send(embed = shopembed)

@client.command() 
async def buy(ctx, publicItemID):
    userID = str(ctx.author.id)
    user = ctx.author
    username = str(user)
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
        else:
            generate_player(userID, username)
            tempPlayer = vault[userID]
        
        if tempPlayer.buy_item(int(publicItemID) - 1) != 'Did not purchase':
            await ctx.send(f'{user.mention} bought {minerIDs[publicItemID - 1]}')
        else:
            await ctx.send(f'{user.mention} is a broke boy and cannot afford a {minerIDs[publicItemID - 1]}')
        vault[userID] = tempPlayer


@client.command()
async def inventory(ctx):
    vaultid = str(ctx.author.id)
    user = ctx.author
    with shelve.open('itemvault') as inventory:
        if vaultid in inventory:
            invEmbed = nextcord.Embed(title = f'Inventory', color=embedBlue)
            i = 0
            invMessage = ''
            for item in inventory[vaultid]:
                if item > 0:
                    invMessage += f'[{item}] {minerIDs[i]}'
                i += 1
            invEmbed.add_field(name='Miners',value=str(invMessage))
            await ctx.send(embed = invEmbed)
        else:
            await ctx.send(f'You have nothing.')

@client.command()
async def mine(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    if userID in cooldowns: #if they have mined before
        if time.time() - cooldowns[userID] >= MINE_COOLDOWN: #if user is ready to mine again
            cooldowns[userID] = time.time() #reset cooldown
            amount = randrange(MINE_MIN, MINE_MAX) #generate amount mined
            mine(userID, amount, str(user))
            await ctx.send(f'you mined {amount} bebbies {user.mention}')
        else:
            await ctx.send(f'too soon man, you gotta wait {(MINE_COOLDOWN - (time.time() - cooldowns[userID]))/60:.1f} minutes before you mine again {user.mention}')
    else:
        cooldowns[userID] = time.time()

        amount = randrange(MINE_MIN, MINE_MAX)
        mine(userID, amount, str(ctx.author))
        await ctx.send(f'you mined {amount} bebbies {user.mention}')

def mine(userID, amount, username):
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            tempPlayer.add_balance(amount)
            vault[userID] = tempPlayer
        else:
            generate_player(userID, username, amount)

def generate_player(userID, username, amt = 0):
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            print('@@@@@USER IS ALREADY IN VAULT@@@@@')
        else:
            vaultname = ''
            for x in range(len(username) - 5):
                vaultname += username[x]
            newPlayer = Player(userID, vaultname)
            vault[userID] = newPlayer
            if amt != 0:
                tempPlayer = vault[userID]
                tempPlayer.add_balance(amt)
                vault[userID] = tempPlayer

@client.command()
async def balance(ctx):
    userID = str(ctx.author.id)
    user = ctx.author
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            userBalance = tempPlayer.get_balance()
        else:
            userBalance = 0

    await ctx.send(f'You have {userBalance} bebbies {user.mention}')

def get_balance(userID):
    with shelve.open('PlayerVault') as vault:
        if userID in vault:
            tempPlayer = vault[userID]
            return tempPlayer.get_balance()
        else:
            generate_player(userID)

@client.command()
async def send(ctx, user: nextcord.User, amt: float):
    userID = str(ctx.author.id)
    recipientID = str(user.id)
    if user and amt:
        with shelve.open('PlayerVault') as vault:
            if userID in vault:
                sendPlayer = vault[userID]
            else:
                generate_player(userID, str(ctx.author))
                sendPlayer = vault[userID]

            if recipientID in vault:
                recipientPlayer = vault[recipientID]
            else:
                generate_player(recipientID, str(user))
                recipientPlayer = vault[recipientID]
            
            if sendPlayer.get_balance() >= amt:
                sendPlayer.add_balance(-amt)
                recipientPlayer.add_balance(amt)
                vault[userID] = sendPlayer
                vault[recipientID] = recipientPlayer
                await ctx.send(f'{ctx.author.mention} has sent {user.mention} {amt} bebbies')


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

#looping for bebbies
@tasks.loop(seconds=5.0)
async def miner_income():
    pass

miner_income.start()

@send.error
async def send_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $send [@user] [amount]')

@buy.error
async def buy_error(ctx, error):
    if isinstance(error, commands.MissingRequiredArgument):
        await ctx.send('Usage: $buy [itemid from shop]')



client.run('OTY4NTc0MDY2MDc0MjEwMzE0.Ymg05A.hfW9WDiZmNV_uoFhFiXChpT0ewU')


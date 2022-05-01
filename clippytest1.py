import nextcord
from nextcord.ext import commands
from nextcord.ext import tasks
import time
import shelve
from random import randrange
import asyncio
from operator import itemgetter


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
MINE_MIN = 300
MINE_MAX = 900
cooldowns = dict() 

# other variables
embedBlue = 0x00daff


# miners stores information about each miner in a list
#   miners[itemid][0] = name
#   miners[itemid][1] = cost
#   miners[itemid][2] = base price
miners = []
with open('miners.txt') as file:
    lines = file.readlines()
    for x in lines:
        oneminer = x.strip('\n').split(',')
        miners.append(oneminer)

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

def get_cost(user, item):
    basePrice = int(miners[item][1])
    #get amount owned by user
    #calculate and return custom cost
    return basePrice

@client.command()
async def shop(ctx):
    shopembed = nextcord.Embed(title = "Bebbies Shop", color=embedBlue)
    num = 0
    for individualminer in miners:
        num += 1
        shopembed.add_field(name=('Tier '+str(num)), value= f'{individualminer[0]}\nProduction: {float(individualminer[2]):,}/second\nCost: {float(individualminer[1]):,} bebbies')
    await ctx.send(embed = shopembed)

@client.command() 
async def buy(ctx, itemid):
    vaultid = str(ctx.author.id)
    user = ctx.author
    username = str(user)
    itemid = int(itemid) - 1
    itemPrice = get_cost(vaultid, itemid)
    with shelve.open('itemvault') as inventory:
        if vaultid in inventory:
            if get_balance(vaultid) >= itemPrice:
                print('@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@')
                set_balance(vaultid, get_balance(vaultid) - itemPrice, username)

                newInventory = []
                for x in range(len(miners)):
                    if itemid == x:
                        newInventory.append(1 + inventory[vaultid][itemid])
                    else:
                        newInventory.append(inventory[vaultid][x])
                inventory[vaultid] = newInventory

                await ctx.send(f"{user.mention} has purchased a(n) {minerIDs[itemid]}")
            else:
                await ctx.send(f"{user.mention} is a broke boy and doesn't have {itemPrice} bebbies")
        else:
            if get_balance(vaultid) >= get_cost(vaultid, itemid):
                set_balance(vaultid, get_balance(vaultid) - itemPrice, username)
                newInventory = []
                for x in range(len(miners)):
                    if itemid == x:
                        newInventory.append(1)
                    else:
                        newInventory.append(0)
                inventory[vaultid] = newInventory
                await ctx.send(f"{user.mention} has purchased a(n) {minerIDs[itemid]}")
            else:
                await ctx.send(f"{user.mention} is a broke boy and doesn't have {itemPrice} bebbies")

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
    vaultid = str(ctx.author.id)
    user = ctx.author
    if vaultid in cooldowns: #if they have mined before
        if time.time() - cooldowns[vaultid] >= MINE_COOLDOWN: #if user is ready to mine again
            cooldowns[vaultid] = time.time() #reset cooldown
            amount = randrange(MINE_MIN, MINE_MAX) #generate amount mined
            mine(vaultid, amount, str(user))
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


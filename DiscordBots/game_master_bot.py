import os
import discord
from dotenv import load_dotenv
import random
import time

load_dotenv()
TOKEN = os.getenv('DISCORD_TOKEN')
GUILD = os.getenv('DISCORD_GUILD')
GAME_MASTER = os.getenv('DISCORD_GAME_MASTER_ROLE')
LOBBY = os.getenv('DISCORD_LOBBY_CHANNEL')
GENERAL = os.getenv('DISCORD_GENERAL_CHANNEL')

client = discord.Client()

class GameSettings:
    def __init__(self):
        self.imposter_count = 1
        self.task_count = 3
        self.kill_cooldown = 30
        self.sabotoge_cooldown = 60

game_settings = GameSettings()

def set_game(imposters=-1, tasks=-1, k_cool=-1, s_cool=-1):
    if imposters != -1:
        game_settings.imposter_count = imposters
    if tasks != -1:
        game_settings.task_count = tasks
    if k_cool != -1:
        game_settings.kill_cooldown = k_cool
    if s_cool != -1:
        game_settings.sabotoge_cooldown = s_cool

class Task:
    def __init__(self, name, crew_count, repeatable, subtask_count, instruction_list, remote_trigger):
        self.name = name
        self.crew_count = crew_count
        self.repeatable = repeatable
        self.subtask_count = subtask_count
        self.instruction_list = instruction_list
        self.remote_trigger = remote_trigger
        self.completed_count = 0

    def get_task(self):
        response = self.name
        if self.completed_count >= self.subtask_count:
            response = response + " - Done"
            return response
        else:
            response = response + "(" + str(self.completed_count) + "/" + str(self.subtask_count) + ")"
            return response

master_task_list = []

class Player_Data:
    def __init__(self, discord_handle, name):
        self.discord_handle = discord_handle
        self.name = name
        self.role = "Player"
        self.alive = True
        self.tasks = []

    def is_alive(self):
        return self.alive

    def set_role(self, role):
        self.role = role

player_list = []

async def send_roster(channel):
    sendstr = "Roster:\n"
    for player in player_list:
        sendstr = sendstr + "\t" + player.name
    await channel.send(sendstr)

reactor_meltding = False
oxygen_depleting = False
last_sabotage_time = 0
last_kill_time = 0
kills_needed = 0
tasks_completed = 0
tasks_remaining = 0
tasks_total = 0

def reactor_meltdown():
    print("reactor melting")
    time.sleep(5)
    print("Five seconds elapsed")

def deplete_oxygen():
    print("Oxygen depleting")

def set_sabotoge_time():
    global last_sabotage_time
    last_sabotage_time = time.time()

def set_kill_time():
    global last_kill_time
    last_kill_time = time.time()

def get_sabotoge_time():
    return time.time() - last_sabotage_time

def get_kill_time():
    return time.time() - last_kill_time

def sabotoge_allowed():
    return get_sabotoge_time() > game_settings.sabotoge_cooldown

def kill_allowed():
    return get_kill_time() > game_settings.kill_cooldown

def get_sabotoge_time_remaining():
    return game_settings.sabotoge_cooldown - get_sabotoge_time()

def get_kill_time_remaining():
    return game_settings.kill_cooldown - get_sabotoge_time()

def kill_victim(victim_name, imposter_channel):
    for player in player_list:
        if player.name == victim_name:
            if player.is_alive():
                player.alive = False
                kills_needed = kills_needed - 1
                await imposter_channel.send("Killing of " + victim_name + " recorded")
                if kills_needed == 0:
                    end_game(True)
                set_kill_time()
                return
            else:
                await imposter_channel.send("Player already dead")
                return
    await imposter_channel.send("No player by that name in the game, type !roster to get player names")

def end_game(imposter_victory):
    pass

@client.event
async def on_ready():
    #Display server info
    guild = client.get_guild(int(GUILD))

    print(
        f'{client.user} is connected to the following guild:\n'
        f'{guild.name}(id: {guild.id})')

    print("Channels:")
    for channel in guild.channels:
        print("\t" + channel.name)

    print("Roles:")
    for role in guild.roles:
        print("\t" + role.name)

    print("Members:")
    members = await guild.fetch_members(limit=150).flatten()
    for member in members:
        print("\t" + member.name)

@client.event
async def on_member_join(member):
    await member.create_dm()
    await member.dm_channel.send("Welcome to the Among Them Discord Server!")

@client.event
async def on_message(message):
    if message.author == client.user:
        return

    #Displays the roster to help with kill reporting
    if message.content == "!roster":
        send_roster(message.channel)
        return

    #Imposter commands
    if message.channel.name == "imposter":
        if message.content == "!help":
            await message.channel.send("Type !kill [user] to claim a kill\nType !reactor to start a reactor meltdown\nType!oxygen to start an oxygen meltdown\nType !win if you believe you have killed all but one other crewmate\nType !roster to display the names of all players in the game\nType !time to see how long before you can initiate a sabotoge or kill another crewmate")

        #Initiate a reactor meltdown if available
        if message.content == "!reactor":
            if sabotoge_allowed():
                set_sabotoge_time()
                guild = client.get_guild(int(GUILD))
                lobby = guild.get_channel(int(LOBBY))
                await lobby.send("Reactor Meltdown!")
            else:
                await message.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")

        #Initiate oxygen depletion if available
        if message.content == "!oxygen":
            if sabotoge_allowed():
                set_sabotoge_time()
                guild = client.get_guild(int(GUILD))
                lobby = guild.get_channel(int(LOBBY))
                await lobby.send("Oxygen depletion immenent!")
            else:
                await message.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")

        #Claim a kill and record it to the list
        if "!kill" in message.content:
            if kill_allowed():
                victim = message.content.split("!kill")[1]
                if victim == "":
                    await message.channel.send("You must mention the player you killed, try again!")
                    return
                kill_victim(victim, message.channel)             
            else:
                await message.channel.send(str(get_kill_time_remaining()) + " seconds before kills are allowed")

        #Display how long until the user can initiate the next sabotoge
        if message.content == "!time":
            if sabotoge_allowed():
                await message.channel.send("Sabotoge ready!")
            else:
                await message.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")
            if kill_allowed():
                await message.channel.send("Kill ready!")
            else:
                await message.channel.send(str(get_kill_time_remaining()) + " seconds before kills are allowed")
        return

    if message.content=="!lobby" and message.channel.name == "game-commands":
        #Feedback for lobby creation
        print("Creating lobby")
        await message.channel.send("Lobby starting!")
        game_settings = GameSettings()
        player_list = []
        guild = client.get_guild(int(GUILD))

        #Clear roles from users
        members = await guild.fetch_members(limit=150).flatten()
        for member in members:
            if member == client.user:
                continue

            for role in member.roles:
                if role != guild.get_role(int(GUILD)) and role != guild.get_role(int(GAME_MASTER)):
                    await member.remove_roles(role)
                    print("Removed " + role.name + " from " + member.name)

        #Clean the lobby
        lobby = guild.get_channel(int(LOBBY))
        async for message in lobby.history(limit=500):
            await message.delete()

        #Announce lobyy prepared
        general = guild.get_channel(int(GENERAL))
        await general.send("Lobby started! Type \"!join\" to join the lobby.")
        return

    #Game setting commands
    if message.channel.name == "game-commands" and "!set" in message.content:
        command = message.content.split()
        if command[1] == "imposters":
            set_game(imposters=int(command[2]))
            await message.guild.get_channel(int(LOBBY)).send("Imposter count set to " + command[2])
            return
        if command[1] == "tasks":
            set_game(tasks=int(command[2]))
            await message.guild.get_channel(int(LOBBY)).send("Task count set to " + command[2])
            return
        if command[1] == "s_cool":
            set_game(s_cool=int(command[2]))
            await message.guild.get_channel(int(LOBBY)).send("Sabotoge count set to " + command[2])
            return
        if command[1] == "k_cool":
            set_game(k_cool=int(command[2]))
            await message.guild.get_channel(int(LOBBY)).send("Kill cooldown set to " + command[2])
            return

    #Game command help
    if message.channel.name == "game-commands" and message.content == "!help":
        command_list = ""
        command_list = command_list + "!lobby to create a fresh lobby\n"
        command_list = command_list + "!start to start the game from the current lobby\n"
        command_list = command_list + "!set to change game settings. The options are\n"
        command_list = command_list + "\t!set imposters [num] to set the number of imposters"
        command_list = command_list + "\t!set tasks [num] to set the number of imposters"
        command_list = command_list + "\t!set k_cool [num] to set the number of imposters"
        command_list = command_list + "\t!set s_cool [num] to set the cooldown for sabotoges"
        await message.channel.send(command_list)

    #Starts a game with all members in the lobby
    if message.content=="!start" and message.channel.name == "game-commands":
        await message.channel.send("Starting Game")
        #await message.author.create_dm()
        #await message.author.dm_channel.send("You have joined the lobby")
        return

    #Allows a member to join the lobby pre-game
    if "!join" in message.content and message.channel.name == "general":
        name = message.content.split("!join")[1]
        if name == "":
            await message.channel.send("You must enter a name after join for the game")
            return
        for player in player_list:
            if player.name == name:
                await message.channel.send("Someone is already using that name!")
                return
            for player in player_list:
                if player.discord_handle == message.author:
                    await message.channel.send("You are already in the lobby!")
                    return

        print("Adding " + name + " to the game")
        if message.author.name != "Gersh":
            await message.author.edit(nick=name)
        player_list.append(Player_Data(message.author, name))

        await message.channel.send(name + " has been added to the game!")
        role = discord.utils.get(message.guild.roles, name="Player")
        await message.author.add_roles(role)
        await send_roster(message.guild.get_channel(int(LOBBY)))
        return

    if message.channel.name == "lobby":
        #Here we will handle voting
        if message.content == "!leave":
            for index in range(len(player_list)):
                if player_list[index].discord_handle == message.author:
                    player_list.remove(player_list[index])
                    return
        pass

    if message.channel.name == "crew":
        #Here we will handle checking on task status
        #Here you can report finishing a task
        #This is where people can report a dead body
        #This is where people can call an emergency meeting (x1)
        pass


client.run(TOKEN)

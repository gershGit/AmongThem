import os
import discord
from discord.ext import commands
from dotenv import load_dotenv
import random
import time
import copy

load_dotenv()
TOKEN = os.getenv('DISCORD_TOKEN')
GUILD = os.getenv('DISCORD_GUILD')
GAME_MASTER = os.getenv('DISCORD_GAME_MASTER_ROLE')
DEVELOPER = os.getenv('DISCORD_DEVELOPER_ROLE')
IMPOSTER = os.getenv('DISCORD_IMPOSTER_ROLE')
CREWMATE = os.getenv('DISCORD_CREWMATE_ROLE')
LOBBY = os.getenv('DISCORD_LOBBY_CHANNEL')
GENERAL = os.getenv('DISCORD_GENERAL_CHANNEL')
IMPOSTER_CHANNEL = os.getenv('DISCORD_IMPOSTER_CHANNEL')

bot = commands.Bot(command_prefix='!')

class GameSettings:
    def __init__(self):
        self.imposter_count = 1
        self.task_count = 3
        self.kill_cooldown = 30
        self.sabotoge_cooldown = 60
        self.meeting_count = 1
        self.door_time_crew = 60
        self.door_time_imposter = 30
        self.meeting_cooldown = 20

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
    def __init__(self, name, short_name, location_list, crew_count, repeatable, subtask_count, instruction_list, remote_trigger):
        self.name = name
        self.short_name = short_name
        self.location_list = location_list
        self.crew_count = crew_count
        self.repeatable = repeatable
        self.subtask_count = subtask_count
        self.instruction_list = instruction_list
        self.remote_trigger = remote_trigger
        self.completed_count = 0

    def is_complete(self):
        return self.completed_count >= self.subtask_count

    def get_task_status(self):
        if self.completed_count >= self.subtask_count:
            return " : Done"
        else:
            return " : (" + str(self.completed_count) + "/" + str(self.subtask_count) + ")"

    def get_current_location(self):
        return self.location_list[self.completed_count]

#IRL task format
#wires -> actually connect
#swipe -> limit switch in slot, time how long it is held down by the swipe
#calibrate -> ?
#align engine -> use knob with extender (rotary encoder)
#chart -> move spaceship along course until it reaches a limit switch
#clean filter -> throw some number of balls into a bin
#clear asteroids -> time your clicks with an LED flash, have another LED track when the gun is "loaded"
#divert power -> flip light switches in two different rooms, switchboard and button in each room
#empty chute -> hold a lever down for a period of time
#fuel engines -> 1. Bring fuel cells to the engine and snap them into place, 2. Fill up engines with water until weight presses a button
#inspect sample -> click button to start, come back after a minute and click button corresponding to red LED
#prime shields -> solve a simple hexagon puzzle
#stabilize steering -> 1. balance object, 2. 2 knobs to accurately move the stearing to the target
#submit scan -> stand in place for some number of seconds, send complete task when you start and when you complete
#start reactor -> click the right buttons corresponding to the led pattern
#unlock manifolds -> figure out which key unlocks the lock
#upload data -> grab 3d printed card and put it in a 3d printed slot

master_task_list = [
    Task("Fix Wires", "wires", ["storage", "cafeteria", "shields"], 1, False, 3, ["Connect the wires from the left to the right", "Connect the wires from the left to the right", "Connect the wires from the left to the right"], "Remote"),
    Task("Swipe Card", "swipe", ["admin"], 1, True, 1, ["Swipe the card through the slot"], "Match"),
    Task("Calibrate Distributor", "calibrate", ["electrical"], 1, True, 1, ["Twist the knob until calibrated"], "Remote"),
    Task("Align Engine Output", "align", ["Upper Engine", "Lower Engine"], 1, False, 2, ["Align the engine output", "Align the engine output"], True),
    Task("Chart course", "chart", ["Navigation"], 1, False, 1, ["Move the spaceship to the final location"], "Remote"),
    Task("Clean O2 Filter", "clean", ["Oxygen"], 1, False, 1, ["Move the leaves into the slot"], "Report"),
    Task("Clear Astroids", "astroids", ["Weapons"], 1, True, 1, ["Shoot 20 asteroids as they appear"], "Match"),
    Task("Divert Power to Communications", "pcomm", ["Electrical", "Communications"], 1, False, 2, ["Flip the power on for the room labeled communications", "Retrieve the power disk from communications and place it onto the main table"], "Report"),
    Task("Divert Power to Lower Engine", "pengine", ["Electrical", "Lower Engine"], 1, False, 2, ["Flip the power on for the room labeled Engines", "Retrieve the power disk from lower engine and place it onto the main table"], "Report"),
    Task("Divert Power to Navigation", "pnav", ["Electrical", "Navigation"], 1, False, 2, ["Flip the power on for the room labeled navigation", "Retrieve the power disk from navigation and place it onto the main table"], "Report"),
    Task("Divert Power to O2", "pox", ["Electrical", "Oxygen"], 1, False, 2, ["Flip the power on for the room labeled oxygen", "Retrieve the power disk from oxygen and place it onto the main table"], "Report"),
    Task("Divert Power to Reactor", "preactor", ["Electrical", "Reactor"], 1, False, 2, ["Flip the power on for the room labeled reactor", "Retrieve the reactor disk from reactor and place it onto the main table"], "Report"),
    Task("Divert Power to Security", "psec", ["Electrical", "Security"], 1, False, 2, ["Flip the power on for the room labeled security", "Retrieve the power disk from security and place it onto the main table"], "Report"),
    Task("Divert Power to Shields", "pshield", ["Electrical", "Shields"], 1, False, 2, ["Flip the power on for the room labeled shields", "Retrieve the power disk from shields and place it onto the main table"], "Report"),
    Task("Divert Power to Upper Engine", "pupper", ["Electrical", "Upper Engine"], 1, False, 2, ["Flip the power on for the room labeled upper engine", "Retrieve the power disk from upper engine and place it onto the main table"], "Report"),
    Task("Empty Chute", "chute", ["Oxygen", "Storage"], 1, True, 2, ["Hold the chute lever down until the light turns green", "Hold the chut lever down until the light turns green"], "Match"),
    Task("Fuel Engines", "fuel", ["Storage", "Upper Engine", "Lower Engine"], 1, False, 4, ["Retrieve a single fuel cell", "Insert fuel cell into upper engine", "Retrieve a single fuel cell", "Insert fuel cell into lower engine"], "Remote"),
    Task("Inspect Sample", "sample", ["MedBay", "MedBay"], 1, True, 2, ["Start the inspection sampler", "Select the incorrect sample"], "Match"),
    Task("Prime Shields", "shields", ["Shields"], 1, False, 1, ["Place all shield discs into the tray"], "Remote"),
    Task("Stabilize stearing", "steering", ["Navigation"], 1, True, 1, ["Align steering with target"], "Match"),
    Task("Submit Scan", "scan", ["Medbay", "Medbay"], 1, True, 2, ["Stand in place on scanner for 20 seconds", "Report scan success"], "Report"),
    Task("Start Reactor", "reactor", ["Reactor"], 1, True, 1, ["Match the pattern of the LEDs"], "Remote"),
    Task("Unlock Manifolds", "unlock", ["Reactor"], 1, True, 1, ["Unlock the manifold"], "Remote"),
    Task("Upload Data from Cafeteria", "datacaf", ["Cafeteria", "Admin"], 1, True, 2, ["Retrieve a data cartridge from the cafeteria", "Place the data cartridge in the admin computer"], "Report"),
    Task("Upload Data from Communications", "datacomm", ["Communications", "Admin"], 1, True, 2, ["Retrieve a data cartridge from communications", "Place the data cartridge in the admin computer"], "Report"),
    Task("Upload Data from Weapons", "dataweapon", ["Weapons", "Admin"], 1, True, 2, ["Retrieve a data cartridge from weapons", "Place the data cartridge in the admin computer"], "Report"),
    Task("Upload Data from Electrical", "dataelec", ["Electrical", "Admin"], 1, True, 2, ["Retrieve a data cartridge from electrical", "Place the data cartridge in the admin computer"], "Report"),
    Task("Upload Data from Navigation", "datanav", ["Navigation", "Admin"], 1, True, 2, ["Retrieve a data cartridge from navigation", "Place the data cartridge in the admin computer"], "Report")
]

class Player_Data:
    def __init__(self, discord_handle, name, color):
        self.discord_handle = discord_handle
        self.name = name
        self.color = color
        self.role = "Player"
        self.alive = True
        self.known_status = True
        self.tasks = []
        self.meetings = 1
        self.vote_available = True
        self.vote = None
        self.kill_cooldown = 30
        self.door_cooldown = 60 
        self.meeting_cooldown = 20
        self.last_door_time = time.time()
        self.last_meeting_time = time.time()
        self.last_kill_time = time.time()

    def is_alive(self):
        return self.alive

    def set_role(self, role):
        self.role = role

    def meeting_allowed(self):
        return self.last_meeting_time - self.meeting_allowed > 0

    def door_allowed(self):
        return self.last_door_time - self.door_cooldown > 0

    def kill_allowed(self):
        return self.last_kill_time - self.kill_cooldown > 0

    def get_meeting_remaining(self):
        return self.meeting_cooldown - self.last_meeting_time

    def get_door_remaining(self):
        return self.door_cooldown - self.last_door_time

    def get_kill_time_remaining(self):
        return self.kill_cooldown - self.last_kill_time

   
color_list = [
    "Red",
    "Green",
    "Cyan",
    "Yellow",
    "Brown",
    "Blue",
    "Pink",
    "Orange"
]
player_list = []
crew_list = []
imposters = []

#Gets the player by their handle
def get_player_by_handle(handle):
    for player in player_list:
        if player.discord_handle == handle:
            return player
    return None

#Gets the player based on their name
def get_player_by_name(name):
    for player in player_list:
        if player.name == name:
            return player
    return None

#Returns a boolean representing if the author is in the list of crewmates
def is_crewmate(handle):
    for crewmate in crew_list:
        if crewmate.discord_handle == handle:
            return True
    return False

#Returns whether or not the discord user is an imposter
def is_imposter(handle):
    for imposter in imposters:
        if imposter.discord_handle == handle:
            return True
    return False

#Returns a boolean for if the message was sent by a crewmate in the private channel
def is_crew_command(ctx):
    return ctx.channel.type == discord.ChannelType.private and is_crewmate(ctx.author)

#Returns a boolean for if the message was sent by a crewmate in the private channel
def is_imposter_command(ctx):
    return ctx.channel.type == discord.ChannelType.private and is_imposter(ctx.author)

async def send_roster(channel):
    sendstr = "Roster:\n"
    for player in player_list:
        sendstr = sendstr + "\t" + player.name + " (" + player.color + ")"           
        if not player.is_alive() and player.known_status():
            sendstr = sendstr = " [Dead]\n"
        else:
            sendstr = sendstr = " [Alive]\n"
    await channel.send(sendstr)

def refresh_game_settings():
    global game_settings
    game_settings = GameSettings()
    player_list.clear()

#Resets the colors
def clear_colors():
    global temp_color_list
    temp_color_list = copy.deepcopy(color_list)

#Selects a random color for the player
def select_color():
    index = random.randrange(0, len(temp_color_list))
    color = temp_color_list[index]
    temp_color_list.remove(color)
    return color

async def clean_chat(channel):
    async for message in channel.history(limit=500):
        await message.delete()

async def clear_roles(member, guild):
    for role in member.roles:
        if role != guild.get_role(int(GUILD)) and role != guild.get_role(int(GAME_MASTER)) and role != guild.get_role(int(DEVELOPER)):
            await member.remove_roles(role)
            print("Removed " + role.name + " from " + member.name)

async def clean_lobby_roles(members, guild):
    for member in members:
        if member == bot.user:
            continue

        for role in member.roles:
            if role != guild.get_role(int(GUILD)) and role != guild.get_role(int(GAME_MASTER)) and role != guild.get_role(int(DEVELOPER)):
                await member.remove_roles(role)
                print("Removed " + role.name + " from " + member.name)

async def check_player_available(name, handle, channel):
    for player in player_list:
        if player.name == name:
            await channel.send("Someone is already using that name!")
            return
        for player in player_list:
            if player.discord_handle == handle:
                await channel.send("You are already in the lobby!")
                return

def add_player_to_lobby(player_data):
    player_list.append(player_data)

async def kick_player(handle, guild):
    for index in range(len(player_list)):
        if player_list[index].discord_handle == handle:
            player_list.remove(player_list[index])
            clear_roles(handle, guild)
            await send_roster(guild.get_channel(LOBBY))
            return

#Randomly chooses an imposter from the list of players and fills the crew list with other members
async def choose_imposter(guild):
    clean_chat(guild.get_channel(int(IMPOSTER_CHANNEL)))

    global imposters, crew_list
    imposters.clear()
    imposter_indices = []
    for _ in range(game_settings.imposter_count):
        index = random.randrange(0, len(player_list))
        if not (index in imposter_indices):
            imposter_indices.append(index)
            imposters.append(player_list[index])
            player_list[index] = player_list[index]
            player_list[index].role = "Imposter"
            player_list[index].door_cooldown = game_settings.door_time_imposter
            player_list[index].meetings = game_settings.meeting_count
            player_list[index].meeting_cooldown = game_settings.meeting_cooldown
            player_list[index].kill_cooldown = game_settings.kill_cooldown
            player_list[index].is_alive = True
            player_list[index].known_status = True
            await player_list[index].discord_handle.add_roles([guild.get_role(int(IMPOSTER)), guild.get_role(int(CREWMATE))])
            await player_list[index].discord_handle.create_dm()
            await player_list[index].discord_handle.dm_channel.send("You are an imposter, wait for the game to start")
    crew_list.clear()
    for x in range(len(player_list)):
        if not x in imposter_indices:
            crew_list.append(player_list[x])
            player_list[x].role = "Crewmate"
            player_list[x].is_alive = True
            player_list[x].meetings = game_settings.meeting_count
            player_list[x].vote_available = False
            player_list[x].door_cooldown = game_settings.door_time_crew
            player_list[x].meeting_cooldown = game_settings.meeting_cooldown
            player_list[x].known_status = True
            await player_list[x].discord_handle.add_roles(guild.get_role(int(CREWMATE)))
            await player_list[x].discord_handle.create_dm()
            await player_list[x].discord_handle.dm_channel.send("You are a crewmate, all your actions will be done from this private message channel. Tasks should arrive momentarily")
    global kills_needed
    kills_needed = len(crew_list) - 1
    global crewmates_alive
    crewmates_alive = len(crew_list)

#Sends the list of tasks to a crewmate
async def send_tasks(crewmate):
    await crewmate.discord_handle.create_dm()
    task_string = "Tasks Remaining:\n"
    for task in crewmate.tasks:
        if not task.is_complete():
            task_string = task_string + task.name + " [" + task.short_name + "] " + task.get_current_location + task.get_task_status + "\n"
    await crewmate.discord_handle.dm_channel.send(task_string)

#Sends tasks to every crewmate
async def send_tasks_to_all():
    for crewmate in crew_list:
        await send_tasks(crewmate)

#Chooses tasks for the crewmates
async def apportion_tasks(guild):
    global tasks_total, tasks_remaining, tasks_completed
    tasks_total = len(crew_list) * game_settings.task_count
    tasks_remaining = len(crew_list) * game_settings.task_count
    tasks_completed = 0
    temp_tasks = master_task_list
    for player in crew_list:
        for _ in range(game_settings.task_count):
            task = temp_tasks[random.randrange(0, len(temp_tasks))]
            player.tasks.append(copy.deepcopy(task))
            if not task.repeatable:
                temp_tasks.remove(task)
    for player in imposters:
        sendstr = "The Imposters are:\n"
        for player in imposters:
            sendstr = sendstr + player.name + " (" + player.color + ")\n"
        await player.discord_handle.dm_channel.send(sendstr)
    
#Starts the cooldowns and sends the okay to move
async def send_start_set_timers():
    set_sabotoge_time()
    for player in imposters:
        player.set_kill_time(time.time())
        player.last_door_time = time.time()
        player.last_meeting_time = time.time()
        await player.discord_handle.dm_channel.send("Sabotoge and kill timers reset, go fake some tasks!")
    for crewmate in crew_list:
        crewmate.last_door_time = time.time()
        crewmate.last_meeting_time = time.time()
        await crewmate.discord_handle.dm_channel.send("Go do tasks!")

#Imposter related code
reactor_melting = False
reactor_top_button = False
reactor_bottom_button = False
oxygen_depleting = False
oxygen_admin = False
oxygen_o2 = False
last_sabotage_time = 0
kills_needed = 0
crewmates_alive = 0

def reactor_meltdown():
    print("reactor melting")
    time.sleep(5)
    print("Five seconds elapsed")

def deplete_oxygen():
    print("Oxygen depleting")

def set_sabotoge_time():
    global last_sabotage_time
    last_sabotage_time = time.time()

def set_kill_time(player):
    global last_kill_time
    last_kill_time = time.time()

def get_sabotoge_time():
    return time.time() - last_sabotage_time

def get_kill_time(player):
    return time.time() - last_kill_time

def sabotoge_allowed():
    return get_sabotoge_time() > game_settings.sabotoge_cooldown

def kill_allowed(player):
    return get_kill_time(player) > game_settings.kill_cooldown

def get_sabotoge_time_remaining():
    return game_settings.sabotoge_cooldown - get_sabotoge_time()

def get_kill_time_remaining(player):
    return game_settings.kill_cooldown - get_sabotoge_time()

async def kill_victim(imposter, victim_name):
    for player in crew_list:
        if player.name == victim_name:
            if player.is_alive():
                player.alive = False
                kills_needed = kills_needed - 1
                victim_player = get_player_by_name(victim_name)
                await victim_player.discord_handle.send("You have been killed! Don't worry, you can still complete your tasks")
                await imposter.dm_channel.send("Killing of " + victim_name + " recorded")
                if kills_needed == 0:
                    end_game(True, bot.get_guild(int(GUILD)))
                set_kill_time(imposter)
                return
            else:
                await imposter.dm_channel.send("Player already dead")
                return
    await imposter.dm_channel.send("No player by that name in the game, type !roster to get player names")

def end_game(imposter_victory, guild):
    pass

#Crew related code
tasks_completed = 0
tasks_remaining = 0
tasks_total = 0

#Returns in string form how many crewmates were alive after the last meeting
def get_known_crew_remaining():
    return str(crewmates_alive)

#Returns the current status of total task completion
def get_task_total_status():
    return str(tasks_completed) + "/" + str(tasks_total)

#Meeting related code
votes = []

@bot.event
async def on_ready():
    #Display server info
    guild = bot.get_guild(int(GUILD))

    print(
        f'{bot.user} is connected to the following guild:\n'
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

#Get information on supported commands using commands
@bot.command(name='commands')
async def help_command(ctx):
    if is_crew_command(ctx):
        helpstr = "Available commands:\n"
        helpstr = helpstr + "`!info` -> displays info on the number of known crewmates remaining and the status of task completion across the ship\n"
        helpstr = helpstr + "`!roster` -> displays the roster with colors and known status\n"
        helpstr = helpstr + "`!tasks` -> displays a list of your remaining tasks\n"
        helpstr = helpstr + "`!inst [taskname]` -> gives instructions on the current step of the task\n"
        helpstr = helpstr + "`!complete [taskname]`-> completes the next step of the task using the shortname of the task\n"
        helpstr = helpstr + "`!body [color]` -> reports a body using the color of the dead marker\n"
        helpstr = helpstr + "`!meeting` -> calls an emergency meeting if available\n"
        helpstr = helpstr + "`!vote [name]` OR `!vote [color]` -> only available during voting periods, allows you to vote for a person (or their color) to be booted from the ship\n"
        helpstr = helpstr + "`!reactor1` OR `!reactor2` -> use when fixing the reactor, only use the one correctly corresponding to your position\n"
        helpstr = helpstr + "`!oxygen1` OR `!oxygen2` -> use when fixing the reactor, only use the one correctly corresponding to your position`\n"
        await ctx.channel.send(helpstr)
        return

    if is_imposter_command(ctx):
        helpstr = helpstr + "`!kill [name]` to claim a kill\n"
        helpstr = helpstr + "`!reactor` to start a reactor meltdown\n"
        helpstr = helpstr + "`!oxygen` to start an oxygen meltdown\n"
        helpstr = helpstr + "`!win` if you believe you have killed all but one other crewmate\n"
        helpstr = helpstr + "`!roster` to display the names of all players in the game\n"
        helpstr = helpstr + "`!time` to see how long before you can initiate a sabotoge or kill another crewmate"
        await ctx.channel.send(helpstr)

    if ctx.channel.name == "discussion":
        pass

    if ctx.channel.name == "lobby":
        pass

    if ctx.channel.name == "dev-command-tests":
        pass

#Crew members can request inoformation using the info commands
@bot.command(name='info')
async def info_command(ctx):
    if is_crew_command(ctx):
        await ctx.message.channel.send(get_known_crew_remaining() + " crewmates alive")
        await ctx.message.channel.send(get_task_total_status() + " tasks")
        return

#Display all of the players, colors, and known status in the game
@bot.command(name='roster')
async def roster_command(ctx):
    await send_roster(ctx.channel)

#Check your task list (!tasks)
@bot.command('tasks')
async def tasks_command(ctx):
    if is_crew_command(ctx):
        await send_tasks(get_player_by_handle(ctx.author))

#Get instructions on a task (!task [name])
#Here you can report finishing a task (!complete [name])
#This is where people can report a dead body (!body)
#This is where people can call an emergency meeting (x1) (!meeting)
#Here we will handle voting (!vote)
#Handle reactor meltdown (!reactor1 or !reactor2)
#Handle oxygen depletion (!oxygen1 or !oxygen2)

#TODO kills and sabotoges need to be player specific for multiple imposters

#Initiate a reactor meltdown if available
@bot.command('reactor')
async def reactor_meltdown_command(ctx):
    if is_imposter_command(ctx):
        if sabotoge_allowed():
            reactor_meltdown()
            set_sabotoge_time()
            for player in player_list:
                await player.discord_handle.dm_channel.send("\n\n\nReactor meltdown immenent!\n\n\n")
        else:
            await ctx.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")

#Initiate oxygen depletion if available
@bot.command('oxygen')
async def oxygen_depletion_command(ctx):
    if is_imposter_command(ctx):
        if sabotoge_allowed():
            set_sabotoge_time()
            deplete_oxygen()
            for player in player_list:
                await player.discord_handle.dm_channel.send("\n\n\nOxygen depletion immenent!\n\n\n")
        else:
            await ctx.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")

#Claim a kill and record it to the list
@bot.command('kill')
async def kill_command(ctx):
    if is_imposter_command(ctx):
        imposter = get_player_by_handle(ctx.author)
        if kill_allowed(imposter):
            victim = ctx.message.content.split("!kill")[1]
            if victim == "":
                await ctx.channel.send("You must mention the player you killed, try again!")
                return
            kill_victim(get_player_by_handle(ctx.author), victim)             
        else:
            await ctx.channel.send(str(get_kill_time_remaining(imposter)) + " seconds before kills are allowed")

#Displays information on the time before players can do certain tasks
@bot.command('time')
async def time_command(ctx):
    if is_imposter_command(ctx):
        imposter = get_player_by_handle(ctx.author)
        if sabotoge_allowed():
            await ctx.channel.send("Sabotoge ready!")
        else:
            await ctx.channel.send(str(get_sabotoge_time_remaining()) + " seconds before sabotoge is available")
        if kill_allowed(imposter):
            await ctx.channel.send("Kill ready!")
        else:
            await ctx.channel.send(str(get_kill_time_remaining(imposter)) + " seconds before kills are allowed")
        if imposter.meeting_allowed():
            await ctx.channel.send("Emergency meeting ready!")
        else:
            await ctx.channel.send(str(imposter.get_meeting_remaining()) + " seconds before meeting is available")
        if imposter.door_allowed():   
            await ctx.channel.send("Door closing ready!")
        else:
            await ctx.channel.send(str(imposter.get_door_remaining()) + " seconds before door closing is available")
        return

    if is_crew_command(ctx):
        crewmate = get_player_by_handle(ctx.author)
        if crewmate.meeting_allowed():
            await ctx.channel.send("Emergency meeting ready!")
        else:
            await ctx.channel.send(str(imposter.get_meeting_remaining()) + " seconds before meeting is available")
        if crewmate.door_allowed():   
            await ctx.channel.send("Door open ready!")
        else:
            await ctx.channel.send(str(imposter.get_door_remaining()) + " seconds before door opening is available")


'''
@bot.event
async def on_message(message):
    #Imposter commands
    if message.channel.name == "imposter":

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
        refresh_game_settings()
        guild = client.get_guild(int(GUILD))

        #Clear colors
        clear_colors()

        #Clear roles from users
        members = await guild.fetch_members(limit=150).flatten()
        await clean_lobby_roles(members, guild)

        #Clean the lobby chat
        lobby = guild.get_channel(int(LOBBY))
        await clean_lobby_chat(lobby)

        #Announce lobby prepared
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
        await message.guild.get_channel(int(LOBBY)).send("Starting Game")
        await choose_imposter(message.guild)
        await apportion_tasks(message.guild)
        await send_tasks_to_all()
        await send_start_set_timers()
        return

    #Allows a member to join the lobby pre-game
    if "!join" in message.content and message.channel.name == "dev-command-tests":
        name = message.content.split("!join")[1]
        if name == "":
            await message.channel.send("You must enter a name after join for the game")
            return
        await check_player_available(name, message.author, message.channel)

        print("Adding " + name + " to the game")
        if message.author.name != "Gersh":
            await message.author.edit(nick=name)
        color = select_color()
        add_player_to_lobby(Player_Data(message.author, name, color))

        await message.channel.send(name + " has been added to the game with color: " + color + "!")
        role = discord.utils.get(message.guild.roles, name="Player")
        await message.author.add_roles(role)
        await send_roster(message.guild.get_channel(int(LOBBY)))
        return

    #The lobby channel is used for in game updates and discussion
    if message.channel.name == "lobby":
        #Here we will handle leaving the lobby
        if message.content == "!leave":
            await kick_player(message.author, message.guild)
            return
'''

bot.run(TOKEN)

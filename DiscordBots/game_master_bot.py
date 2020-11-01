import os
import discord
from discord.ext import commands
from dotenv import load_dotenv
import random
import time
import copy
import asyncio
from threading import Thread
from asynctimer import AsyncTimer
import socket
import stat_system as stats

load_dotenv()
TOKEN = os.getenv('DISCORD_TOKEN')
GUILD = os.getenv('DISCORD_GUILD')
GAME_MASTER = os.getenv('DISCORD_GAME_MASTER_ROLE')
DEVELOPER = os.getenv('DISCORD_DEVELOPER_ROLE')
IMPOSTER = os.getenv('DISCORD_IMPOSTER_ROLE')
CREWMATE = os.getenv('DISCORD_CREWMATE_ROLE')
PLAYER = os.getenv('DISCORD_PLAYER_ROLE')
LOBBY = os.getenv('DISCORD_LOBBY_CHANNEL')
GENERAL = os.getenv('DISCORD_GENERAL_CHANNEL')
IMPOSTER_CHANNEL = os.getenv('DISCORD_IMPOSTER_CHANNEL')

address = ('192.168.0.14', 8771)
python_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
bot = commands.Bot(command_prefix='.')

class GameSettings:
    def __init__(self):
        self.imposter_count = 1
        self.task_count = 3
        self.kill_cooldown = 30
        self.sabotoge_cooldown = 60
        self.oxygen_length = 60
        self.reactor_length = 60
        self.meeting_count = 1
        self.meeting_cooldown = 20
        self.door_time_crew = 60
        self.door_time_imposter = 30
        self.meeting_return_time = 30
        self.meeting_discussion_time = 30
        self.meeting_voting_time = 60
        self.eject_info = True
        self.reporting_type = "Manual" #TODO implement

game_settings = GameSettings()
meeting_status = False
voting_active = False
voting_timer = None
warning_timer = None
skipped_votes = 0
casted_votes = 0

def set_game(imposters=-1, tasks=-1, m_count=-1, k_cool=-1, s_cool=-1, m_cool=-1, d_cool_crew=-1, d_cool_imposter=-1, ox_time=-1, reactor_time=-1, return_time=-1, discussion_time=-1, vote_time=-1, eject_info=-1):
    if imposters != -1:
        game_settings.imposter_count = imposters
    if tasks != -1:
        game_settings.task_count = tasks
    if m_count != -1:
        game_settings.meeting_count = m_count
    if k_cool != -1:
        game_settings.kill_cooldown = k_cool
    if s_cool != -1:
        game_settings.sabotoge_cooldown = s_cool
    if m_cool != -1:
        game_settings.meeting_cooldown = m_cool
    if d_cool_crew != -1:
        game_settings.door_time_crew = d_cool_crew
    if d_cool_imposter != -1:
        game_settings.door_time_imposter = d_cool_imposter
    if ox_time != -1:
        game_settings.oxygen_length = ox_time
    if reactor_time != -1:
        game_settings.reactor_length = reactor_time
    if return_time != -1:
        game_settings.meeting_return_time = return_time
    if discussion_time != -1:
        game_settings.meeting_discussion_time = discussion_time
    if vote_time != -1:
        game_settings.meeting_voting_time = vote_time
    if eject_info != -1:
        if eject_info == 0:
            game_settings.eject_info = False
        if eject_info == 1:
            game_settings.eject_info = True

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
        if (not self.is_complete()):
            return self.location_list[self.completed_count]

    def get_instruction(self):
        if (not self.is_complete()):
            return self.instruction_list[self.completed_count]

    def complete_step(self):
        if self.completed_count >= self.subtask_count:
            return False
        self.completed_count = self.completed_count + 1
        return True

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
    Task("Fuel Engines", "fuel", ["Storage", "Upper Engine", "Storage", "Lower Engine"], 1, False, 4, ["Retrieve a single fuel cell", "Insert fuel cell into upper engine", "Retrieve a single fuel cell", "Insert fuel cell into lower engine"], "Remote"),
    Task("Inspect Sample", "sample", ["MedBay", "MedBay"], 1, True, 2, ["Start the inspection sampler", "Select the incorrect sample"], "Match"),
    Task("Prime Shields", "shields", ["Shields"], 1, False, 1, ["Place all shield discs into the tray"], "Remote"),
    Task("Stabilize stearing", "steering", ["Navigation"], 1, False, 1, ["Align steering with target"], "Match"),
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
    def __init__(self, discord_handle, name, nation):
        self.discord_handle = discord_handle
        self.name = name
        self.nation = nation
        self.role = "Player"
        self.is_alive = True
        self.death_known = False
        self.tasks = []
        self.meetings = 1
        self.vote_available = True
        self.vote = None
        self.votes_against = 0
        self.kill_cooldown = 30
        self.door_cooldown = 60
        self.meeting_cooldown = 20
        self.last_door_time = time.time()
        self.last_meeting_time = time.time()
        self.last_kill_time = time.time()

    def set_role(self, role):
        self.role = role

    def meeting_allowed(self):
        if self.meetings > 0 and self.is_alive:
            return self.get_meeting_remaining() <= 0
        return False

    def door_allowed(self):
        return self.get_door_remaining() <= 0

    def kill_allowed(self):
        return self.get_kill_time_remaining() <=0

    def get_elapsed_meeting_time(self):
        return time.time() - self.last_meeting_time

    def get_elapsed_kill_time(self):
        return time.time() - self.last_kill_time

    def get_door_elapsed(self):
        return time.time() - self.last_door_time

    def get_meeting_remaining(self):
        return self.meeting_cooldown - self.get_elapsed_meeting_time()

    def get_door_remaining(self):
        return self.door_cooldown - self.get_door_elapsed()

    def get_kill_time_remaining(self):
        return self.kill_cooldown - self.get_elapsed_kill_time()

    def set_kill_time(self, time_in):
        self.last_kill_time = time_in

nation_dict = {
    "asa" : {"color": "white", "nation": "australia", "flag": ":flag_au:"},
    "cnsa" : {"color": "yellow", "nation": "china", "flag": ":flag_cn:"},
    "esa" : {"color": "blue", "nation": "europe", "flag": ":flag_eu:"},
    "iran" : {"color": "green", "nation": "iran", "flag": ":flag_ir:"},
    "isa" : {"color": "cyan", "nation": "israel", "flag": ":flag_il:"},
    "asi" : {"color": "purple", "nation": "italy", "flag": ":flag_it:"},
    "kcst" : {"color": "orange", "nation": "northkorea", "flag": ":flag_kp:"},
    "kari" : {"color": "brown", "nation": "southkorea", "flag": ":flag_kr:"},
    "isro" : {"color": "black", "nation": "india", "flag": ":flag_in:"},
    "jaxa" : {"color": "magenta", "nation": "japan", "flag": ":flag_jp:"},
    "nasa" : {"color": "pink", "nation": "usa", "flag": ":flag_us:"},
    "cnes" : {"color": "lime", "nation": "france", "flag": ":flag_fr:"},
    "ssau" : {"color": "navy", "nation": "ukraine", "flag": ":flag_ua:"},
    "ros" : {"color": "tan", "nation": "russia", "flag": ":flag_ru:"},
}

nation_list = [
    "asa", #Australia
    "cnsa", #China
    "esa", #Europe
    "iran", #Iran
    "isa", #Israel
    "asi", #Italy
    "kcst", #North Korea
    "kari", #South Korea
    "isro", #India
    "jaxa", #Japan
    "nasa", #USA
    "cnes", #France
    "ssau", #Ukraine
    "ros", #Russia
]
player_list = []
crew_list = []
imposters = []
last_sabotage_time = 0
kills_needed = 0
crewmates_alive = 0
imposters_alive = 0

#Makes a name all lower case with no more than 10 letters no spaces
def clean_name(dirty_name):
    lowered = dirty_name.lower()
    newname = ""
    i=0
    for letter in lowered:
        if letter.isalpha():
            newname = newname + letter
            i=i+1
            if i==10:
                break
    return newname

#Gets the player by their handle
def get_player_by_handle(handle):
    global player_list
    for player in player_list:
        if player.discord_handle == handle:
            return player
    return None

#Gets the player based on their name
def get_player_by_name(name):
    global player_list
    for player in player_list:
        if player.name == name:
            return player
    return None

#Gets the player based on their nation
def get_player_by_color(nation):
    global player_list
    for player in player_list:
        if player.nation == nation:
            return player
    return None

#Returns the number of players alive (including the imposters)
def get_players_alive():
    count = 0
    global player_list
    for player in player_list:
        if player.is_alive:
            count = count + 1
    return count

#Returns a boolean representing if the author is in the list of crewmates
def is_crewmate(handle):
    global crew_list
    for crewmate in crew_list:
        if crewmate.discord_handle == handle:
            return True
    return False

#Returns whether or not the discord user is an imposter
def is_imposter(handle):
    global imposters
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

#Sends the current roster to the specified channel
async def send_roster(channel, full_visibility):
    print("Sending roster")
    if full_visibility:
        print("Request has full visibility access")
    sendstr = "Roster:\n"
    global player_list
    for player in player_list:
        sendstr = sendstr + "\t" + player.name + " " + nation_dict[player.nation]["flag"] + " (" + player.nation + ") "
        if (not player.is_alive) and (player.death_known or full_visibility):
            sendstr = sendstr + " :skull:\n"
        else:
            sendstr = sendstr + " :heart:\n"
    await channel.send(sendstr)

#Resets the settings for the game and empties the list of active players
def refresh_game_settings():
    global game_settings, player_list
    game_settings = GameSettings()
    player_list = []

#Resets the colors
def clear_colors():
    global temp_color_list
    temp_color_list = copy.deepcopy(nation_list)

#Selects a random nation for the player
def select_color():
    global nation_list, player_list
    nation = ""
    while True:
        found = False
        index = random.randrange(0, len(nation_list))
        nation = nation_list[index]
        for player in player_list:
            if player.nation == nation:
                found = True
        if not found:
            break
    return nation

#Deletes all the chats from a channel
async def clean_chat(channel):
    async for message in channel.history(limit=500):
        await message.delete()

#Removes all but the specified safe roles from a member
async def clear_roles_from_member(member, guild):
    for role in member.roles:
        if role != guild.get_role(int(GUILD)) and role != guild.get_role(int(GAME_MASTER)) and role != guild.get_role(int(DEVELOPER)):
            await member.remove_roles(role)
            print("Removed " + role.name + " from " + member.name)

#Removes all but the safe roles from all of the members
async def reset_roles(members, guild, safe_roles):
    for member in members:
        if member == bot.user:
            continue

        for role in member.roles:
            if (not (role in safe_roles)):
                await member.remove_roles(role)
                print("Removed " + role.name + " from " + member.name)

#Checks if the name is free and the player isn't already in the lobby
async def check_player_available(name, handle, channel):
    for player in player_list:
        if player.name == name:
            await channel.send("Someone is already using that name!")
            return False
        for player in player_list:
            if player.discord_handle == handle:
                await channel.send("You are already in the lobby!")
                return False
    return True

#Places the player in the lobby with permission to access the channel
def add_player_to_lobby(player_data):
    player_list.append(player_data)

#Removes the player from the list and removes their access to the lobby
async def kick_player(handle, guild):
    for index in range(len(player_list)):
        if player_list[index].discord_handle == handle:
            player_list.remove(player_list[index])
            clear_roles_from_member(handle, guild)
            await send_roster(guild.get_channel(LOBBY), True)
            return

#Resets all players to only have player role and clears the chat
#All other reset data happens on !start command
async def reset_lobby():
    lobby_channel = bot.get_guild(int(GUILD)).get_channel(int(LOBBY))
    await clean_chat(lobby_channel)
    guild = bot.get_guild(int(GUILD))
    members = await guild.fetch_members(limit=150).flatten()
    safe_roles = [
        guild.get_role(int(GUILD)),
        guild.get_role(int(GAME_MASTER)),
        guild.get_role(int(DEVELOPER)),
        guild.get_role(int(PLAYER))
    ]
    await reset_roles(members, guild, safe_roles)
    await lobby_channel.send("Lobby reset, start game when ready!")

#Randomly chooses an imposter from the list of players and fills the crew list with other members
async def choose_imposter(guild):
    global imposters, crew_list, imposters_alive, player_list, game_settings
    imposters_alive = game_settings.imposter_count
    imposters = []
    imposter_indices = []
    for x in range(game_settings.imposter_count):
        index = random.randrange(0, len(player_list))
        if index not in imposter_indices:
            imposter_indices.append(index)
            imposters.append(player_list[index])
            imposters[x] = player_list[index]
            imposters[x].role = "Imposter"
            imposters[x].door_cooldown = game_settings.door_time_imposter
            imposters[x].meetings = game_settings.meeting_count
            imposters[x].meeting_cooldown = game_settings.meeting_cooldown
            imposters[x].kill_cooldown = game_settings.kill_cooldown
            player_list[index].is_alive = True
            player_list[index].death_known = False
            await imposters[x].discord_handle.add_roles(guild.get_role(int(IMPOSTER)))
            await imposters[x].discord_handle.create_dm()
            await imposters[x].discord_handle.dm_channel.send(".\n\n\n\n\n\n\n\n\n\nYou are an imposter, wait for the game to start")
    crew_list = []
    for x in range(len(player_list)):
        if x not in imposter_indices:
            crew_list.append(player_list[x])
            player_list[x].role = "Crewmate"
            player_list[x].meetings = game_settings.meeting_count
            player_list[x].vote_available = False
            player_list[x].door_cooldown = game_settings.door_time_crew
            player_list[x].meeting_cooldown = game_settings.meeting_cooldown
            player_list[x].is_alive = True
            player_list[x].death_known = False
            await player_list[x].discord_handle.add_roles(guild.get_role(int(CREWMATE)))
            await player_list[x].discord_handle.create_dm()
            await player_list[x].discord_handle.dm_channel.send(".\n\n\n\n\n\n\n\n\n\nYou are a crewmate, all your actions will be done from this private message channel. Tasks should arrive momentarily")
    global kills_needed
    kills_needed = len(crew_list) - 1
    global crewmates_alive
    crewmates_alive = len(player_list)

#Sends the list of tasks to a crewmate
async def send_tasks(crewmate):
    print("Sending tasks to " + crewmate.name)
    task_string = "\n\n\n\nTasks Remaining:\n"
    for task in crewmate.tasks:
        if not task.is_complete():
            task_string = task_string + task.name + " [" + task.short_name + "] " + task.get_current_location() + " " + task.get_task_status() + "\n"
    await crewmate.discord_handle.dm_channel.send(task_string)

#Sends tasks to every crewmate
async def send_tasks_to_all():
    print("Sending all crewmates their tasks...")
    for crewmate in crew_list:
        await send_tasks(crewmate)

#Chooses tasks for the crewmates
async def apportion_tasks(guild):
    print("Choosing tasks")
    global tasks_total, tasks_completed
    tasks_total = 0
    tasks_completed = 0
    temp_tasks = master_task_list

    #Give every player in the crew list the right number of tasks
    #Crew takes precedent over imposter fake tasks to reduce the number of repeatable tasks between crew members
    for player in crew_list:
        player.tasks = [] #Reset the players task list to 0

        #Loop enough times to fill the players task list
        for _ in range(game_settings.task_count):
            task = temp_tasks[0]
            while True:
                #Select a random task from the remaining list
                task = temp_tasks[random.randrange(0, len(temp_tasks))]

                #Ensure the player does not get 2 of the exact same task, even if it is a repeatable task
                for t in player.tasks:
                    #Check if the task selected is one of the tasks already in the list
                    if t.short_name == task.short_name:
                        #Continue the loop, thereby reselecting a random task from the remaining list
                        continue
                #Break out of the while loop if no duplicate of the task was found
                break
            #Increase the total task count by the number of subtasks
            tasks_total = tasks_total + task.subtask_count

            #Copy the task into the players list, (Must deepcopy so repeatable tasks don't share status)
            player.tasks.append(copy.deepcopy(task))

            #If the task cannot be repeated by separate players then remove it from the list of selectable tasks
            if not task.repeatable:
                temp_tasks.remove(task)

    for imposter in imposters:
        imposter.tasks = []
        sendstr = "The Imposters are:\n"
        for imp in imposters:
            sendstr = sendstr + imp.name + " (" + imp.nation + ")\n"
        await imposter.discord_handle.dm_channel.send(sendstr)

        #Find some fake tasks for the imposter to do
        for _ in range(game_settings.task_count):
            task = temp_tasks[random.randrange(0, len(temp_tasks))]
            imposter.tasks.append(copy.deepcopy(task))
            if not task.repeatable:
                temp_tasks.remove(task)
        await imposter.discord_handle.dm_channel.send("Your fake tasks are:")
        await send_tasks(imposter)

#Starts the cooldowns and sends the okay to move
async def send_start_set_timers():
    print("Resetting timers and sending go signal")
    set_sabotoge_time()
    for player in imposters:
        player.last_kill_time = time.time()
        player.last_door_time = time.time()
        player.last_meeting_time = time.time()
        await player.discord_handle.dm_channel.send("Sabotoge and kill timers reset, go fake some tasks!")
    for crewmate in crew_list:
        crewmate.last_door_time = time.time()
        crewmate.last_meeting_time = time.time()
        await crewmate.discord_handle.dm_channel.send("Go do tasks!")

#Imposter related code
reactor_melting = False
reactor_top_fixed = False
reactor_top_player = None
reactor_bottom_fixed = False
reactor_bottom_player = None
oxygen_depleting = False
oxygen_a_fixed = False
oxygen_b_fixed = False
oxygen_admin = False
oxygen_o2 = False
reactor_timer = None
reactor_warning = None
oxygen_timer = None
oxygen_warning = None

#Handles an attempt to fix the reactor at a location, records who is currently at that location for kill purposes
async def fix_reactor(location, fixer):
    global reactor_top_fixed, reactor_bottom_fixed, reactor_timer, reactor_warning, reactor_bottom_player, reactor_top_player
    if (location == "Top"):
        reactor_top_fixed = True
        reactor_top_player = fixer
    if (location == "Bottom"):
        reactor_bottom_fixed = True
        reactor_bottom_player = fixer
    if reactor_top_fixed and reactor_bottom_fixed:
        if reactor_timer != None:
            reactor_timer.cancel()
        if reactor_warning != None:
            reactor_warning.cancel()
        for player in player_list:
            await player.discord_handle.dm_channel.send(">\n\n-----------------------------------------\n|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|\n|.......................................|\n|                                       |\n| Launch aborted. Return to your tasks! |\n|                                       |\n|.......................................|\n|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|\n-----------------------------------------\n")
        return True
    return False

#Sends the ten second warning to fix the reactor
async def send_reactor_warning():
    global reactor_melting
    if (reactor_melting == True):
        for player in player_list:
            await player.discord_handle.dm_channel.send(">\n\n-------------------------\n| Launch in 10 seconds! |\n-------------------------")

#Ends the game due to failure to fix the reactor meltdown
async def send_reactor_melted():
    global reactor_melting
    reactor_melting = False
    for player in player_list:
        await player.discord_handle.dm_channel.send("--------------------------------------------------------\n| Return rocket launched. All astronauts are stranded. |\n--------------------------------------------------------\n")
    await end_game(">\n\n------------------\n| IMPOSTERS win! |\n------------------")

#Starts the process of reactor meltdown
async def reactor_meltdown():
    print("Reactor melting")
    global reactor_timer, reactor_warning, reactor_melting
    launchstr = ">\n\n"
    launchstr = launchstr + "--------------------------------------------------------------\n"
    launchstr = launchstr + "|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|\n"
    launchstr = launchstr + "|////////////////////////////////////////////////////////////|\n"
    launchstr = launchstr + "|                                                            |\n"
    launchstr = launchstr + "| Launch sequence started. Abort it in the control room now! |\n"
    launchstr = launchstr + "|                                                            |\n"
    launchstr = launchstr + "|////////////////////////////////////////////////////////////|\n"
    launchstr = launchstr + "|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|\n"
    launchstr = launchstr + "--------------------------------------------------------------\n"
    for player in player_list:
        await player.discord_handle.dm_channel.send(launchstr)
    reactor_melting = True

    #Send the data to the server
    '''
    print("Connecting bot to server")
    HOST = '172.16.18.150'
    PORT = 8771
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.connect((HOST, PORT))
    server_sock.sendall(b'>-ReactorTop-MELTDOWN')
    server_sock.close()
    '''

    reactor_warning = AsyncTimer(game_settings.reactor_length-10, send_reactor_warning)
    reactor_timer = AsyncTimer(game_settings.reactor_length, send_reactor_melted)

#Handles an attempt to fix oxygen at the specified location
async def fix_oxygen(location):
    global oxygen_a_fixed, oxygen_b_fixed, oxygen_timer, oxygen_warning
    if (location == "A"):
        oxygen_a_fixed = True
    if (location == "B"):
        oxygen_b_fixed = True
    if oxygen_a_fixed and oxygen_b_fixed:
        if oxygen_timer != None:
            oxygen_timer.cancel()
        if oxygen_warning != None:
            oxygen_warning.cancel()
        for player in player_list:
            await player.discord_handle.dm_channel.send("Oxygen levels fixed, return to your tasks")
        return True
    return False

#Sends the ten second warning about the oxygen depletion levels
async def send_oxygen_warning():
    global oxygen_depleting
    if (oxygen_depleting == True):
        for player in player_list:
            await player.discord_handle.dm_channel.send(">\n\n-----------------------------------\n| Oxygen depletion in 10 seconds! |\n-----------------------------------")

#Ends the game from depleted oxygen not being fixed
async def send_oxygen_depleted():
    global oxygen_depleting
    oxygen_depleting = False
    for player in player_list:
        await player.discord_handle.dm_channel.send(">\n\n----------------------------------------\n| Oxygen depleted. All crewmates died. |\n----------------------------------------")
    await end_game(">\n\n------------------\n| IMPOSTERS win! |\n------------------")

#Begins the oxygen depletion sabotoge
async def deplete_oxygen():
    print("Oxygen depleting")
    global oxygen_timer, oxygen_warning, oxygen_depleting
    depletionstr = ">\n\n"
    depletionstr = depletionstr + "-----------------------------------------------------\n"
    depletionstr = depletionstr + "|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|\n"
    depletionstr = depletionstr + "|///////////////////////////////////////////////////|\n"
    depletionstr = depletionstr + "|                                                   |\n"
    depletionstr = depletionstr + "| Airlocks opened! Close both before time runs out! |\n"
    depletionstr = depletionstr + "|                                                   |\n"
    depletionstr = depletionstr + "|///////////////////////////////////////////////////|\n"
    depletionstr = depletionstr + "|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|\n"
    depletionstr = depletionstr + "-----------------------------------------------------\n"
    for player in player_list:
        await player.discord_handle.dm_channel.send(depletionstr)
    oxygen_depleting = True
    oxygen_warning = AsyncTimer(game_settings.oxygen_length-10, send_oxygen_warning)
    oxygen_timer = AsyncTimer(game_settings.oxygen_length, send_oxygen_depleted)

#Sets the sabotoges last time to the current time
def set_sabotoge_time():
    global last_sabotage_time
    last_sabotage_time = time.time()

#Gets the time since the last sabotoge
def get_sabotoge_time_elapsed():
    return time.time() - last_sabotage_time

#Boolean function that returns whether or not sabotoge is ready
def sabotoge_allowed():
    return get_sabotoge_time_elapsed() > game_settings.sabotoge_cooldown

#Checks how much time is left before an imposter can sabotoge
def get_sabotoge_time_remaining():
    return game_settings.sabotoge_cooldown - get_sabotoge_time_elapsed()

#Handles the logic for killing another player
async def kill_victim(imposter, victim_name):
    print("Attempting victim kill")
    global player_list
    for victim in player_list:
        if victim.name == victim_name:
            if victim.is_alive:
                #Ensure the kill isnt another imposter or themselves
                if victim.role == "Imposter":
                    await imposter.discord_handle.dm_channel.send("You can not kill imposters! (Including yourself!)")
                    return

                victim.is_alive = False
                global kills_needed, reactor_melting

                #Handle the case where the player was killed while fixing a reactor meltdown
                if (reactor_melting):
                    global reactor_bottom_fixed, reactor_bottom_player, reactor_top_fixed, reactor_top_player
                    if (reactor_top_player.name == victim.name):
                        reactor_top_fixed = False
                        reactor_top_player = None
                    if (reactor_bottom_player.name == victim.name):
                        reactor_bottom_fixed = False
                        reactor_bottom_player = None

                #Update the kills needed to win the match and alert the player of the kill validation
                kills_needed = kills_needed - 1
                print("Kill success -> " + str(kills_needed) + " kills remaining")
                await imposter.discord_handle.dm_channel.send("Killing of " + victim_name + " recorded")

                #Alert the killed player of their updated status
                await victim.discord_handle.send(">\n\n```---------------------------------------------\n|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|\n|>>>   >>  You have been killed.   <<    <<<|\n| Don't worry you can still complete tasks! |\n|...........................................|\n---------------------------------------------```\n")

                #End the game if necessary
                if kills_needed == 0:
                    await end_game("IMPOSTERS killed all crewmates needed and win!")

                #Reset the kill timer on the imposter
                imposter.set_kill_time(time.time())
                return
            else:
                await imposter.discord_handle.dm_channel.send("Player already dead")
                return
    await imposter.discord_handle.dm_channel.send("No player by that name in the game, type `.roster` to get player names")

#Announces the end of the game and resets the lobby
async def end_game(statement):
    for player in player_list:
        await player.discord_handle.dm_channel.send(statement)
        await player.discord_handle.dm_channel.send("Return to the lobby channel as the game resets!")
    await reset_lobby()

async def handle_action(action_info):
    print("Handling action " + action_info)
    if action_info == "Meeting":
        global meeting_status
        if meeting_status == True:
            return
        for player in player_list:
            meeting_string = "An emergency meeting has been called!\n"
            meeting_string = meeting_string + "You have " + str(game_settings.meeting_return_time) + " seconds to return to the command console"
            await player.discord_handle.dm_channel.send(meeting_string)
        start_meeting_timers()

async def complete_task_arduino(task_info):
    print("Handling arduino task completion of " + task_info)
    print("Logged completion of task!")

#Crew related code
tasks_completed = 0
tasks_total = 0

#Returns in string form how many crewmates were alive after the last meeting
def get_known_crew_remaining():
    return str(crewmates_alive)

#Returns the current status of total task completion
def get_task_total_status():
    return str(tasks_completed) + "/" + str(tasks_total)

#Sends the votes against this player to all players
async def send_player_votes(crewmate):
    for player in player_list:
        await player.discord_handle.dm_channel.send(str(crewmate.votes_against) + " vote(s) for " + crewmate.name)

#Sends everyone a notification that a player has voted
async def alert_vote(caster):
    global casted_votes
    cast_string = "\n------------------------------\n"
    cast_string = cast_string + caster.name + " voted!\n"
    cast_string = cast_string + str(get_players_alive() - casted_votes) + " remaining\n"
    cast_string = cast_string + "------------------------------\n"
    for player in player_list:
        await player.discord_handle.dm_channel.send(cast_string)

#Sends each player a notification of who was expelled and if they were the imposter
async def send_expelled(expelled):
    infostr = expelled.name
    if expelled.role == "Imposter":
        infostr = infostr + " was an IMPOSTER"
    else:
        infostr = infostr + " was NOT an imposter"
    for player in player_list:
        await player.discord_handle.dm_channel.send(expelled.name + " was launched out the airlock")
        if game_settings.eject_info:
            await player.discord_handle.dm_channel.send(infostr)

#Sends each player a notification that the vote was skipped
async def send_voting_skipped():
    for player in player_list:
        await player.discord_handle.dm_channel.send("Voting was skipped")

#Counts the votes, expells a player, ends the meeting, resets all timers
async def count_votes():
    global meeting_status, voting_active, crewmates_alive
    if meeting_status == False:
        print("Function called early by all players voting!")
    print("Tallying votes")
    meeting_status = False
    voting_active = False
    max_vote = 0
    expelled = None
    for player in player_list:
        if player.votes_against > 0:
            await send_player_votes(player)
            if player.votes_against > max_vote:
                max_vote = player.votes_against
                expelled = player
    for crew in player_list:
        if skipped_votes > 0:
            await crew.discord_handle.dm_channel.send(str(skipped_votes) + " votes to skip")
    if max_vote > skipped_votes:
        expelled.death_known = True
        await send_expelled(expelled)
        if expelled.role == "Imposter":
            global imposters_alive
            imposters_alive = imposters_alive - 1
            crewmates_alive = crewmates_alive - 1
            if imposters_alive == 0:
                await end_game("All imposters voted out, CREWMATES win!")
                return
        else:
            global kills_needed
            kills_needed = kills_needed - 1
            crewmates_alive = crewmates_alive - 1
            if kills_needed == 0:
                await end_game("All crewmates are dead except the imposters final meal. IMPOSTERS win!")
                return
    else:
        await send_voting_skipped()
    await send_tasks_to_all()
    await send_start_set_timers()

#Sends a warning to all players that haven't voted that they now need to vote
async def send_vote_warning():
    for player in player_list:
        if player.vote_available == True and player.is_alive == True:
            await player.discord_handle.dm_channel.send("10 Seconds remaining to vote!")

#Announces to all living players that they can send in their vote
async def announce_voting_period():
    print("Starting voting")
    global voting_active
    voting_active = True
    global voting_timer, warning_timer, casted_votes, skipped_votes
    skipped_votes = 0
    casted_votes = 0
    for player in player_list:
        if player.is_alive == True:
            player.vote_available = True
            await player.discord_handle.dm_channel.send("Voting now active! Cast your vote using `.vote [name]`")
        else:
            await player.discord_handle.dm_channel.send("Living players are now voting!")
    warning_timer = AsyncTimer(game_settings.meeting_voting_time - 10, send_vote_warning)
    voting_timer = AsyncTimer(game_settings.meeting_voting_time, count_votes)

#Announces to all living players they can talk
async def announce_discussion_period():
    print("Start discussion")
    for player in player_list:
        if player.is_alive == True:
            await player.discord_handle.dm_channel.send("Discussion period has started, sus out the imposter!")
        else:
            await player.discord_handle.dm_channel.send("Living players are now discussing!")
    print("Sent all discussion notifications")
    AsyncTimer(game_settings.meeting_discussion_time, announce_voting_period)

#Controls a meeting with timing info
def start_meeting_timers():
    print("Meeting started")
    for player in player_list:
        if (not player.is_alive) and (not player.death_known):
            player.death_known = True
            global crewmates_alive
            crewmates_alive = crewmates_alive - 1
    AsyncTimer(game_settings.meeting_return_time, announce_discussion_period)

@bot.event
async def on_ready():
    #Display server info
    guild = bot.get_guild(int(GUILD))
    print(
        f'{bot.user} is connected to the following guild:\n'
        f'{guild.name}(id: {guild.id})')

#Get information on supported commands using commands
@bot.command(name='commands')
async def help_command(ctx):
    if is_crew_command(ctx):
        helpstr = "Available commands:\n"
        helpstr = helpstr + "`.info` -> displays info on the number of known crewmates remaining and the status of task completion across the ship\n"
        helpstr = helpstr + "`.roster` -> displays the roster with colors and known status\n"
        helpstr = helpstr + "`.tasks` -> displays a list of your remaining tasks\n"
        helpstr = helpstr + "`.how [taskname]` -> gives instructions on the current step of the task\n"
        helpstr = helpstr + "`.complete [taskname]`-> completes the next step of the task using the short name of the task\n"
        helpstr = helpstr + "`.body [nation]` -> reports a body using the nation of the dead marker\n"
        helpstr = helpstr + "`.meeting` -> calls an emergency meeting if available\n"
        helpstr = helpstr + "`.vote [name]` OR `.vote [nation]` -> only available during voting periods, allows you to vote for a person (or their nation) to be booted from the ship\n"
        helpstr = helpstr + "`.reactortop` OR `.reactorbot` -> use when fixing the reactor, only use the one correctly corresponding to your position\n"
        helpstr = helpstr + "`.oxygena` OR `.oxygenb` -> use when fixing the reactor, only use the one correctly corresponding to your position`\n"
        await ctx.channel.send(helpstr)
        return

    if is_imposter_command(ctx):
        helpstr = ""
        helpstr = helpstr + "`.kill [name]` to claim a kill\n"
        helpstr = helpstr + "`.reactor` to start a reactor meltdown\n"
        helpstr = helpstr + "`.oxygen` to start an oxygen meltdown\n"
        helpstr = helpstr + "`.roster` to display the names of all players in the game\n"
        helpstr = helpstr + "`.time` to see how long before you can initiate a sabotoge or kill another crewmate"
        await ctx.channel.send(helpstr)

    #Catch error commands for other private messages
    if ctx.channel.type == discord.ChannelType.private:
        return

    if ctx.channel.name == "discussion":
        pass

    if ctx.channel.name == "lobby":
        helpstr = ""
        helpstr = helpstr + "`.name [name]` to cahnge your name\n"
        helpstr = helpstr + "`.nation [nation]` to change your nation\n"
        helpstr = helpstr + "`.leave` to leave the lobby\n"
        helpstr = helpstr + "`.roster` to display the names of all players in the game\n"
        await ctx.channel.send(helpstr)

    if ctx.channel.name == "game-commands":
        command_list = "Commands:\n"
        command_list = command_list + "`.lobby` to create a fresh lobby\n"
        command_list = command_list + "`.start` to start the game from the current lobby\n"
        command_list = command_list + "`.set` to change game settings. The options are\n"
        command_list = command_list + \
            "\t`.set imposters [num]` to set the number of imposters\n"
        command_list = command_list + \
            "\t`.set tasks [num]` to set the number of imposters\n"
        command_list = command_list + \
            "\t`.set meetings [num]` to set the number of meetings each player can call\n"
        command_list = command_list + \
            "\t`.set k_cool [num]` to set the number of imposters\n"
        command_list = command_list + \
            "\t`.set s_cool [num]` to set the cooldown for sabotoges\n"
        command_list = command_list + \
            "\t`.set d_cool_crew [num]` to set the cooldown for crew to open doors\n"
        command_list = command_list + \
            "\t`.set d_cool_imposter [num]` to set the cooldown for imposters to close doors\n"
        command_list = command_list + \
            "\t`.set m_cool [num]` to set the cooldown for meetings\n"
        command_list = command_list + \
            "\t`.set ox_time [num]` to set how long it takes for the oxygen to deplete\n"
        command_list = command_list + \
            "\t`.set reactor_time [num]` to set how long it takes for the reactor to melt\n"
        command_list = command_list + \
            "\t`.set return_time [num]` to set how long players have to return to the main room before disussion begins\n"
        command_list = command_list + \
            "\t`.set discussion_time [num]` to set how long players have to discuss during a meeting before voting begins\n"
        command_list = command_list + \
            "\t`.set vote_time [num]` to set how long players have to vote during a meeting\n"
        command_list = command_list + \
            "\t`.set eject_info true` or `.set eject_info false` to set whether or not ejected players roles are revealed\n"
        await ctx.channel.send(command_list)

    if ctx.channel.name == "dev-command-tests":
        pass

#Crew members can request inoformation using the info commands
@bot.command(name='info')
async def info_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx):
        global meeting_status
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        await ctx.channel.send(get_known_crew_remaining() + " crewmates alive")
        await ctx.channel.send(get_task_total_status() + " tasks")
        return

#Display all of the players, colors, and known status in the game
@bot.command(name='roster')
async def roster_command(ctx):
    if is_imposter_command(ctx):
        await send_roster(ctx.channel, True)
    else:
        caller = get_player_by_handle(ctx.author)
        if caller.is_alive:
            await send_roster(ctx.channel, False)
        else:
            await send_roster(ctx.channel, True)

#Check your task list (!tasks)
@bot.command('tasks')
async def tasks_command(ctx):
    global meeting_status
    if is_crew_command(ctx):
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        await send_tasks(get_player_by_handle(ctx.author))
        return
    if is_imposter_command(ctx):
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        await ctx.channel.send("THESE ARE FAKE TASKS")
        await send_tasks(get_player_by_handle(ctx.author))
        return

#Get details on how to do a task
@bot.command('how')
async def task_instructions_command(ctx):
    if is_crew_command(ctx):
        global meeting_status
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        taskname = ctx.message.content.split()[1]
        crewmate = get_player_by_handle(ctx.author)
        for task in crewmate.tasks:
            if task.short_name == taskname:
                await ctx.author.dm_channel.send(task.get_instruction())

#Here you can report finishing a task
@bot.command('complete')
async def task_completion_command(ctx):
    if is_crew_command(ctx):
        global meeting_status
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        taskname = ctx.message.content.split()[1]
        crewmate = get_player_by_handle(ctx.author)
        for task in crewmate.tasks:
            if task.short_name == taskname:
                if task.complete_step() == True:
                    global tasks_completed, tasks_total
                    tasks_completed = tasks_completed + 1
                    await ctx.author.dm_channel.send("Task completion recorded")
                    await send_tasks(crewmate)
                    if (tasks_completed == tasks_total):
                        await end_game("CREWMATES completed all tasks and win!")
                        return
                else:
                    await ctx.author.dm_channel.send("Task already completed")
        return
    if is_imposter_command(ctx):
        await ctx.channel.send("IMPOSTERS CANNOT COMPLETE TASKS!")

#TODO reporting body should allow for nation, color, or agency
#This is where people can report a dead body (!body)
@bot.command('body')
async def body_report_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx):
        if (not get_player_by_handle(ctx.author).is_alive):
            await ctx.channel.send("Dead people cannot report bodies!")
        global meeting_status
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        crewmate = get_player_by_handle(ctx.author)
        body = get_player_by_color(ctx.message.content.split()[1])
        if body == None:
            await ctx.channel.send("No player with that nation!")
            return
        if (body.is_alive):
            await ctx.channel.send("Check your reporting, make sure you have the right nation")
            return
        if body.death_known:
            await ctx.channel.send("This body is already known to be dead!")
            return
        body.death_known = True
        meeting_status = True

        #Set the status of sabotoges to off
        global reactor_melting, oxygen_depleting, reactor_timer, reactor_warning, oxygen_timer, oxygen_warning
        global reactor_top_fixed, reactor_bottom_fixed, oxygen_a_fixed, oxygen_b_fixed
        reactor_melting = False
        oxygen_depleting = False
        oxygen_a_fixed = False
        oxygen_b_fixed = False
        reactor_top_fixed = False
        reactor_bottom_fixed = False

        #Set the timers for the sabotoges to cancel so no end-game is launched
        if oxygen_timer != None:
            oxygen_timer.cancel()
        if oxygen_warning != None:
            oxygen_warning.cancel()
        if reactor_timer != None:
            reactor_timer.cancel()
        if reactor_warning != None:
            reactor_warning.cancel()

        #Alert the player of the succesful reporting
        await ctx.channel.send("Body reported, calling a meeting!")

        #Alert all players that a body was found and that they must now return to the meeting room
        for player in player_list:
            meeting_string = "A body has been reported!\n"
            meeting_string = meeting_string + crewmate.name + " reported the body of " + body.name + "!\n"
            meeting_string = meeting_string + "You have " + str(game_settings.meeting_return_time) + " seconds to return to the command console"
            await player.discord_handle.dm_channel.send(meeting_string)

        #Begin the set of timers that control the timing of discussion and votes
        start_meeting_timers()

#This is where people can report a dead body (!body)
@bot.command('meeting')
async def call_meeting_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx) and (not oxygen_depleting) and (not reactor_melting):
        global meeting_status
        if meeting_status == True:
            await ctx.channel.send("Can't do that during a meeting!")
            return
        crewmate = get_player_by_handle(ctx.author)
        if crewmate.meeting_allowed():
            crewmate.meetings = crewmate.meetings - 1
            meeting_status = True
            await ctx.channel.send("Meeting success, alerting others!")
            for player in player_list:
                meeting_string = "An emergency meeting has been called by " + crewmate.name + "!\n"
                meeting_string = meeting_string + "You have " + str(game_settings.meeting_return_time) + " seconds to return to the command console"
                await player.discord_handle.dm_channel.send(meeting_string)
            start_meeting_timers()
        else:
            if crewmate.meetings == 0:
                await crewmate.discord_handle.dm_channel.send("You are out of emergency meetings!")
            else:
                await crewmate.discord_handle.dm_channel.send("You must wait {:.0f} more seconds before you can call a meeting".format(crewmate.get_meeting_remaining()))

#Here we will handle voting (!vote)
@bot.command('vote')
async def vote_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx):
        caster = get_player_by_handle(ctx.author)
        if (not voting_active):
            await ctx.channel.send("Voting is not active at this time!")
            return
        if (not caster.is_alive):
            await ctx.channel.send("Dead players are not allowed to vote!")
            return
        if (not caster.vote_available):
            await ctx.channel.send("You have already casted your vote")
            return
        name = ctx.message.content.split()[1]
        player_to_vote = get_player_by_name(name)

        if name == "skip":
            await ctx.channel.send("You voted to skip!")
            global skipped_votes
            skipped_votes = skipped_votes + 1
        elif player_to_vote == None:
            await ctx.channel.send("Invalid name. Use `.vote [name]` or `.vote skip`\nYou can check player names with `.roster`")
            return
        else:
            player_to_vote.votes_against = player_to_vote.votes_against + 1

        caster.vote_available = False
        global casted_votes, meeting_status
        casted_votes = casted_votes + 1

        await alert_vote(caster)
        if casted_votes == get_players_alive():
            global warning_timer, voting_timer
            meeting_status = False
            if warning_timer != None:
                warning_timer.cancel()
            if voting_timer != None:
                voting_timer.cancel()
            await count_votes()

#Handle reactor meltdown (!reactortop or !reactorbot)
@bot.command('reactortop')
async def reactor_top_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx) and (reactor_melting == True):
        if (not get_player_by_handle(ctx.author).is_alive):
            await ctx.channel.send("Dead players cannot fix sabotoges!")
            return
        await fix_reactor("Top", get_player_by_handle(ctx.author))
@bot.command('reactorbot')
async def reactor_bot_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx) and (reactor_melting == True):
        if (not get_player_by_handle(ctx.author).is_alive):
            await ctx.channel.send("Dead players cannot fix sabotoges!")
            return
        await fix_reactor("Bottom", get_player_by_handle(ctx.author))
#Handle oxygen depletion (!oxygena or !oxygenb)
@bot.command('oxygena')
async def oxygen_a_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx) and (reactor_melting == True):
        if (not get_player_by_handle(ctx.author).is_alive):
            await ctx.channel.send("Dead players cannot fix sabotoges!")
            return
        is_fixed = await fix_oxygen("A")
        if (not is_fixed):
            for player in player_list:
                await player.discord_handle.dm_channel.send("Oxygen in room A fixed\nROOM B STILL NEEDS FIXED!")
@bot.command('oxygenb')
async def oxygen_b_command(ctx):
    if is_crew_command(ctx) or is_imposter_command(ctx) and (oxygen_depleting == True):
        if (not get_player_by_handle(ctx.author).is_alive):
            await ctx.channel.send("Dead players cannot fix sabotoges!")
            return
        is_fixed = await fix_oxygen("B")
        if (not is_fixed):
            for player in player_list:
                await player.discord_handle.dm_channel.send("Oxygen in room B fixed\nROOM A STILL NEEDS FIXED!")

#Handle opening and closing of doors
@bot.command('door')
async def door_command(ctx):
    if is_imposter_command(ctx) and (not meeting_status):
        imposter = get_player_by_handle(ctx.author)
        if imposter.door_allowed():
            imposter.last_door_time = time.time()
            await ctx.channel.send("Door closure confirmed")
        else:
            await ctx.channel.send("You have {:.0f} seconds remaining before you can close a door".format(imposter.get_door_remaining()))
    if is_crew_command(ctx) and (not meeting_status):
        crew = get_player_by_handle(ctx.author)
        if crew.door_allowed():
            crew.last_door_time = time.time()
            await ctx.channel.send("Door opening confirmed")
        else:
            await ctx.channel.send("You have {:.0f} seconds remaining before you can open a door".format(crew.get_door_remaining()))

#Initiate a reactor meltdown if available
@bot.command('reactor')
async def reactor_meltdown_command(ctx):
    if is_imposter_command(ctx):
        if sabotoge_allowed() and (not meeting_status):
            reactor_meltdown()
            set_sabotoge_time()
            for player in player_list:
                await player.discord_handle.dm_channel.send("\n\n\nReactor meltdown immenent!\n\n\n")
        else:
            await ctx.channel.send("You must wait {:.0f} more seconds before you can sabotoge".format(get_sabotoge_time_remaining()))

#Initiate oxygen depletion if available
@bot.command('oxygen')
async def oxygen_depletion_command(ctx):
    if is_imposter_command(ctx) and (not meeting_status):
        if sabotoge_allowed():
            set_sabotoge_time()
            await deplete_oxygen()
            return
        else:
            await ctx.channel.send("You must wait {:.0f} more seconds before you can sabotoge".format(get_sabotoge_time_remaining()))

#TODO killing should allow for name, nation, color, or agency
#Claim a kill and record it to the list
@bot.command('kill')
async def kill_command(ctx):
    if is_imposter_command(ctx) and (not meeting_status):
        imposter = get_player_by_handle(ctx.author)
        if imposter.kill_allowed():
            victim = ctx.message.content.split()[1]
            if victim == "":
                await ctx.channel.send("You must mention the player you killed, try again!")
                return
            await kill_victim(get_player_by_handle(ctx.author), victim)
        else:
            await ctx.channel.send("You must wait {:.0f} more seconds before you can kill".format(imposter.get_kill_time_remaining()))

#Displays information on the time before players can do certain tasks
@bot.command('time')
async def time_command(ctx):
    if is_imposter_command(ctx):
        imposter = get_player_by_handle(ctx.author)
        if sabotoge_allowed():
            await ctx.channel.send("Sabotoge ready!")
        else:
            await ctx.channel.send("{:.0f} seconds before you can sabotoge".format(get_sabotoge_time_remaining()))
        if imposter.kill_allowed():
            await ctx.channel.send("Kill ready!")
        else:
            await ctx.channel.send("{:.0f} seconds before you can kill".format(imposter.get_kill_time_remaining()))
        if imposter.meeting_allowed():
            await ctx.channel.send("Emergency meeting ready!")
        else:
            await ctx.channel.send("{:.0f} seconds before you can call a meeting".format(imposter.get_meeting_remaining()))
        if imposter.door_allowed():
            await ctx.channel.send("Door closing ready!")
        else:
            await ctx.channel.send("{:.0f} seconds before you can close a door".format(imposter.get_door_remaining()))
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

#Starts a fresh lobby
@bot.command('lobby')
async def lobby_create_command(ctx):
    if ctx.channel.name == "game-commands":
        #Feedback for lobby creation
        print("Creating lobby")
        await ctx.channel.send("Lobby created!")
        refresh_game_settings()
        guild = bot.get_guild(int(GUILD))

        #Clear colors
        clear_colors()

        #Clear roles from users
        members = await guild.fetch_members(limit=150).flatten()
        safe_roles = [
            guild.get_role(int(GUILD)),
            guild.get_role(int(GAME_MASTER)),
            guild.get_role(int(DEVELOPER))
        ]
        await reset_roles(members, guild, safe_roles)

        #Clean the lobby chat
        lobby = guild.get_channel(int(LOBBY))
        await clean_chat(lobby)

        #Load up the statistics file
        stats.load_stats_file()

        #Announce lobby prepared
        general = guild.get_channel(int(GENERAL))
        await general.send("Lobby created! Type `.join [name]` to join the lobby.")

        connection.sendall(b'Lobby\n')
        return

#Game setting commands
@bot.command('set')
async def settings_command(ctx):
    if ctx.channel.name == "game-commands":
        request = ctx.message.content.split()
        if request[1] == "imposters":
            set_game(imposters=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Imposter count set to " + request[2])
            return
        if request[1] == "tasks":
            set_game(tasks=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Task count set to " + request[2])
            return
        if request[1] == "s_cool":
            set_game(s_cool=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Sabotoge count set to " + request[2])
            return
        if request[1] == "k_cool":
            set_game(k_cool=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Kill cooldown set to " + request[2])
            return
        if request[1] == "d_cool_crew":
            set_game(d_cool_crew=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Door cooldown for crewmates set to " + request[2])
            return
        if request[1] == "d_cool_imposter":
            set_game(d_cool_imposter=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Door cooldown for imposters set to " + request[2])
            return
        if request[1] == "m_count":
            set_game(m_count=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Meeting count set to " + request[2])
            return
        if request[1] == "m_cool":
            set_game(m_cool=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Meeting cooldown set to " + request[2])
            return
        if request[1] == "ox_time":
            set_game(ox_time=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Oxygen depletion time set to " + request[2])
            return
        if request[1] == "reactor_time":
            set_game(reactor_time=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Reactor meltdown time set to " + request[2])
            return
        if request[1] == "return_time":
            set_game(return_time=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Return time set to " + request[2])
            return
        if request[1] == "discussion_time":
            set_game(discussion_time=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Discussion time set to " + request[2])
            return
        if request[1] == "vote_time":
            set_game(vote_time=int(request[2]))
            await ctx.guild.get_channel(int(LOBBY)).send("Voting time set to " + request[2])
            return
        if request[1] == "eject_info":
            if request[2] == "true":
                set_game(eject_info=1)
                await ctx.guild.get_channel(int(LOBBY)).send("Ejection info set to TRUE")
                return
            elif request[2] == "false":
                set_game(eject_info=0)
                await ctx.guild.get_channel(int(LOBBY)).send("Ejection info set to FALSE")
                return

#Game starting command, all players that have joined the lobby will be part of the game
@bot.command('start')
async def start_game_command(ctx):
    #TODO also allow from lobby if player is game-master
    if ctx.channel.type != discord.ChannelType.private and ctx.channel.name == "game-commands":
        print("Starting game")
        await choose_imposter(ctx.guild)
        await apportion_tasks(ctx.guild)
        await send_tasks_to_all()
        await send_start_set_timers()
        return

 #Allows a member to join the lobby pre-game

#Adds a player to the lobby, they will play when the start command is sent
@bot.command('join')
async def join_lobby_command(ctx):
    if ctx.channel.type != discord.ChannelType.private and ctx.channel.name == "discussion":
        name = clean_name(ctx.message.content.split()[1])
        if name == "":
            await ctx.channel.send("You must enter a name after `.join`")
            return
        available = await check_player_available(name, ctx.author, ctx.channel)
        if available == False:
            return

        print("Adding " + name + " to the game")
        if ctx.author.name != "Gersh":
            await ctx.author.edit(nick=name)
        nation = select_color()
        add_player_to_lobby(Player_Data(ctx.author, name, nation))
        register_statistics(ctx.author)

        await ctx.guild.get_channel(int(LOBBY)).send(name + " has been added to the game with nation: " + nation + "!")
        role = discord.utils.get(ctx.guild.roles, name="Player")
        await ctx.author.add_roles(role)
        await send_roster(ctx.guild.get_channel(int(LOBBY)), True)
        return

#Allows a player to change their name in the lobby without leaving and rejoining
@bot.command('name')
async def name_change_command(ctx):
    if ctx.channel.type != discord.ChannelType.private and ctx.channel.name == "lobby":
        name = ctx.message.content.split()[1].lower()
        if name == "":
            await ctx.channel.send("You must enter a name after `.name` to change your name")
            return
        player = get_player_by_handle(ctx.author)
        if player == None:
            return

        print("Changing to name:  " + name)
        player.name = name
        await ctx.author.edit(nick=name)
        await send_roster(ctx.guild.get_channel(int(LOBBY)), True)
        return

#Allows a player to change their nation once in the lobby
@bot.command('nation')
async def color_change_command(ctx):
    if ctx.channel.type != discord.ChannelType.private and ctx.channel.name == "lobby":
        nation = ctx.message.content.split()[1].lower()
        if nation == "":
            await ctx.channel.send("You must enter a nation after `.nation` to change your nation")
            return

        temp = get_player_by_handle(ctx.author)
        if temp == None:
            return

        global player_list
        for player in player_list:
            if player.nation == nation:
                await ctx.channel.send(nation + " is already selected by a player!")
                return

        if not nation in nation_list:
            await ctx.channel.send(nation + " is not a valid nation, nation must be:\n asa, cnsa, esa, iran, isa, asi, kcst, kari, isro, jaxa, nasa, cnes, ssau, ros")
            return
        print("Changing " + temp.name + " to " + nation)
        temp.nation = nation

        await ctx.guild.get_channel(int(LOBBY)).send(temp.name + " has changed their nation to: " + nation + "!")
        await send_roster(ctx.guild.get_channel(int(LOBBY)), True)
        return

#Allows a player to leave the lobby
@bot.command('leave')
async def leave_lobby_command(ctx):
    if ctx.channel.type != discord.ChannelType.private and ctx.channel.name == "lobby":
        await kick_player(ctx.author, ctx.guild)
        return

TCP_IP = '172.16.18.150'
ARDUINO_PORT = 8771
DISCORD_PORT = 8772

arduino_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
arduino_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) 
arduino_server.setblocking(False)
arduino_server.bind((TCP_IP, ARDUINO_PORT)) 

connection = None

#Grabs data from the server to pass to the bot
async def periodic_polling():
    socket.create_connection(address)
    await bot.wait_until_ready()
    print("Background task connected, starting server")
    arduino_server.listen(1)
    print("Wating for connection from arduino hub")
    global connection
    while True:
        try:
            connection, client_address = arduino_server.accept()
            print("Connection from ", client_address)
            while True:
                data_bytes = None
                try:
                    data_bytes=connection.recv(2048)
                    print("Server received bytes: ", data_bytes)
                    data = str(data_bytes.decode().strip())
                    print("Data is: " + data)
                    print("Data[0] is: ", data[0])
                    if (data[0] == "R"):
                        print ("Register")
                        connection.sendall(b'Test\n')
                    if (data[0] == "A"):
                        await handle_action(data[2:])
                    if (data[0] == "T"):
                        await complete_task_arduino(data[2:])
                except socket.error:
                    await asyncio.sleep(0.25)
                    pass
        except:
            await asyncio.sleep(0.25)
            continue
    print("Loop finished? Oh no!")

#Runs the polling task and the bot
bot.loop.create_task(periodic_polling())
print("Running bot startup")
bot.run(TOKEN)

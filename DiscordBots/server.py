import socket
import discord
from threading import Thread 
from socketserver import ThreadingMixIn 
import time
from asynctimer import AsyncTimer
import asyncio
from dotenv import load_dotenv
import os

load_dotenv()
TOKEN = os.getenv('DISCORD_TOKEN')
GUILD = os.getenv('DISCORD_GUILD')

discord_data_flag = False
discord_data_list = []
discord_data_lock = asyncio.Lock()

discord_bot_lock = asyncio.Lock()
discord_bot = None

reactor_top_ip = ""
reactor_top_port = 0
reactor_top_registered = False

client = discord.Client()

@client.event
async def on_ready():
    #Display server info
    guild = client.get_guild(int(GUILD))
    #TODO send any message here
    client.close()

# Multithreaded Python server : TCP Server Socket Thread Pool
class ClientThread(Thread): 
    #TODO add more complex information so each thread can handle the connection based on what type of client is speaking
    def __init__(self,ip,port): 
        Thread.__init__(self) 
        self.ip = ip 
        self.port = port 
        self.uniqueID = 0
        self.taskID = "Unknown"
        print("[+] New server socket thread started for " + ip + ":" + str(port))

        #When we need to manage messages here is where we will
        #MESSAGE = (b'MELTDOWN\n')
        #conn.send(MESSAGE)  
 
    #TODO launch different functions depending on what type of client it is
    async def run(self): 
        while True : 
            global discord_data_lock, discord_data_list, discord_data_flag
            data_bytes = None
            try:
                data_bytes=conn.recv(2048)
                print("Server received bytes: ", data_bytes)
            except socket.error:
                #TODO here we should poll the game state to do what needs to be done
                async with discord_data_lock:
                    if discord_data_flag:
                        for d_data in discord_data_list:
                            if d_data[0] == self.taskID:
                                conn.send(d_data[1].encode())
                                discord_data_list.remove(d_data)
                time.sleep(0.25)
                continue
            data = str(data_bytes.decode().strip())
            print ("\nServer received data:", data)
            if (len(data) == 0):
                print("Ignoring empty message")
                continue
            if data[0] == "R":
                print("Registering object")
                global reactor_top_ip, reactor_top_port, reactor_top_registered
                reactor_top_ip = self.ip
                reactor_top_port = self.port
                reactor_top_registered = True
                self.taskID = "ReactorTop"
                continue
            if data[0] == "V":
                print("Maintaining Connection")
                continue
            if data[0] == "F":
                print("Logging fix")
                continue
            if data[0] == "D":
                print("Logging release")
                continue
            if data[0] == "U":
                print("Unregistering client")
                continue
            if data[0] == ">":
                print("Handling discord data")
                async with discord_data_lock:
                    message_split = data.split('-')
                    instr = [message_split[1], message_split[2]]
                    discord_data_list.append(instr)
                    discord_data_flag = True
                continue
            print("Unknown message")

# Multithreaded Python server : TCP Server Socket Program Stub
TCP_IP = '172.16.18.150'
TCP_PORT = 8771
BUFFER_SIZE = 20  # Usually 1024, but we need quick response 

tcpServer = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
tcpServer.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) 
tcpServer.setblocking(False)
tcpServer.bind((TCP_IP, TCP_PORT)) 
threads = [] 

print("Multithreaded Python server : Waiting for connections from TCP clients..." )
while True: 
    tcpServer.listen(20) 
    try:
        (conn, (ip,port)) = tcpServer.accept() 
    except:
        time.sleep(0.25)
        continue
    newthread = ClientThread(ip,port) 
    newthread.start() 
    threads.append(newthread) 
 
for t in threads: 
    t.join() 

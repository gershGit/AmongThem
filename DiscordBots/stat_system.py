#This file handles the statistics portion of the game
titles = []
stats_array = []

def save_stats():
	f = open("statistics.csv", "w")
	write_str = ""
	for x in range(len(titles)):
		if x == len(titles) - 1:
			write_str = write_str + str(titles[x]) + "\n"
			break
		write_str = write_str + str(titles[x]) + ","
	f.write(write_str)
	for player in stats_array:
		write_str = ""
		for x in range(len(player)):
			if x == len(player) - 1:
				write_str = write_str + str(player[x]) + "\n"
				break
			write_str = write_str + str(player[x]) + ","
		f.write(write_str)
	f.close()

def load_stats_file():
	global titles, stats_array
	f = open("statistics.csv", "r")
	titles = f.readline().strip().split(',')
	for line in f:
		player_stats = line.strip().split(',')
		stats_array.append(player_stats)
	f.close()
	f = open("stats_old.csv", "w")
	write_str = ""
	for x in range(len(titles)):
		if x == len(titles) - 1:
			write_str = write_str + str(titles[x]) + "\n"
			break
		write_str = write_str + str(titles[x]) + ","
	f.write(write_str)
	for player in stats_array:
		write_str = ""
		for x in range(len(player)):
			if x == len(player) - 1:
				write_str = write_str + str(player[x]) + "\n"
				break
			write_str = write_str + str(player[x]) + ","
		f.write(write_str)
	f.close()
	print("Loaded the following stats: ")
	print(titles)

def register_statistics(discord_handle):
	for player in stats_array:
		if player[0] == discord_handle:
			return True
	blank_stats = []
	for _ in titles:
		blank_stats.append(0)
	blank_stats[0] = discord_handle
	stats_array.append(blank_stats)
	save_stats()


bool checkFlag(byte flag, byte set) {
	return flag & set;
}

byte clearFlag(byte flag, byte set) {
	return (!flag) & set;
}

byte setFlag(byte flag, byte set) {
	return flag | set;
}

void connectToServer() {

}

void waitForResume() {

}
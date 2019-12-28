#include "server.h"
#include "file.h"
#include "util/GSFileParser.h"
#include "tags.h"
#include "gameset/gameset.h"
#include "network.h"
#include "terrain.h"

Server *Server::instance = nullptr;

void Server::loadSaveGame(const char * filename)
{
	char *filetext; int filesize;
	LoadFile(filename, &filetext, &filesize, 1);
	filetext[filesize] = 0;
	GSFileParser gsf(filetext);

	while (!gsf.eof)
	{
		std::string strtag = gsf.nextTag();
		int tag = Tags::SAVEGAME_tagDict.getTagID(strtag.c_str());

		switch (tag) {
		case Tags::SAVEGAME_GAME_SET: {
			std::string gsFileName = gsf.nextString(true);
			gameSet = new GameSet(gsFileName.c_str());
			NetPacketWriter msg(NETCLIMSG_GAME_SET);
			msg.writeStringZ(gsFileName);
			sendToAll(msg);
			break;
		}
		case Tags::SAVEGAME_NEXT_UNIQUE_ID:
			nextUniqueId = gsf.nextInt();
			break;
		case Tags::SAVEGAME_PREDEC:
			loadSavePredec(gsf); break;
		case Tags::SAVEGAME_LEVEL:
			level = loadObject(gsf, strtag);
			break;
		case Tags::SAVEGAME_TIME_MANAGER_STATE:
			timeManager.load(gsf);
			break;
		}
		gsf.advanceLine();
	}

	free(filetext);

	timeManager.unlock();
	timeManager.unpause();
}

ServerGameObject* Server::createObject(GameObjBlueprint * blueprint, uint32_t id)
{
	if (!id)
		id = nextUniqueId++;
	ServerGameObject *obj = new ServerGameObject(id, blueprint);
	idmap[id] = obj;

	obj->subtype = rand() % blueprint->subtypeNames.size();

	NetPacketWriter msg(NETCLIMSG_OBJECT_CREATED);
	msg.writeUint32(id);
	msg.writeUint32(blueprint->getFullId());
	msg.writeUint32(obj->subtype);
	sendToAll(msg);

	return obj;
}

ServerGameObject* Server::loadObject(GSFileParser & gsf, const std::string &clsname)
{
	int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(clsname.c_str());
	int goid = gsf.nextInt();
	std::string bpname = gsf.nextString(true);

	GameObjBlueprint *blueprint;
	if (bpname.empty())
		blueprint = predec[goid];
	else
		blueprint = gameSet->findBlueprint(cls, bpname);

	ServerGameObject *obj = createObject(blueprint, goid);

	gsf.advanceLine();
	std::string endtag = "END_" + clsname;
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		int tag = Tags::GAMEOBJ_tagDict.getTagID(strtag.c_str());
		switch (tag) {
		case Tags::GAMEOBJ_ITEM: {
			std::string itemstr = gsf.nextString(true);
			int item = gameSet->itemNames.getIndex(itemstr);
			if (item != -1)
				obj->setItem(item, gsf.nextFloat());
			else
				printf("WARNING: Item \"%s\" doesn't exist!\n", itemstr.c_str());
			break;
		}
		case Tags::GAMEOBJ_POSITION: {
			float x = gsf.nextFloat();
			float y = gsf.nextFloat();
			float z = gsf.nextFloat();
			obj->setPosition(Vector3(x, y, z));
			break;
		}
		case Tags::GAMEOBJ_ORIENTATION: {
			float x = gsf.nextFloat();
			float y = gsf.nextFloat();
			float z = gsf.nextFloat();
			obj->setOrientation(Vector3(x, y, z));
			break;
		}
		case Tags::GAMEOBJ_MAP: {
			std::string mapfp = gsf.nextString(true);
			this->terrain = new Terrain();
			this->terrain->readBCM(mapfp.c_str());
			NetPacketWriter packet(NETCLIMSG_TERRAIN_SET);
			packet.writeStringZ(mapfp);
			sendToAll(packet);
			break;
		}
		case Tags::GAMEOBJ_COLOUR_INDEX: {
			obj->setColor(gsf.nextInt());
			break;
		}
		case Tags::GAMEOBJ_SUBTYPE: {
			obj->setSubtypeAndAppearance(obj->blueprint->subtypeNames.getIndex(gsf.nextString(true)), obj->appearance);
			break;
		}
		case Tags::GAMEOBJ_APPEARANCE: {
			std::string subtype = gsf.nextString(true);
			std::string appear = gsf.nextString(true);
			obj->setSubtypeAndAppearance(obj->blueprint->subtypeNames.getIndex(subtype), gameSet->appearanceNames.getIndex(appear));
			break;
		}
		case Tags::GAMEOBJ_PLAYER:
		case Tags::GAMEOBJ_CHARACTER:
		case Tags::GAMEOBJ_BUILDING:
		case Tags::GAMEOBJ_MARKER:
		case Tags::GAMEOBJ_CONTAINER:
		case Tags::GAMEOBJ_PROP:
		case Tags::GAMEOBJ_CITY:
		case Tags::GAMEOBJ_TOWN:
		{
			ServerGameObject *child = loadObject(gsf, strtag);
			child->setParent(obj);
		}
		}
		if (strtag == endtag)
			return obj;
		gsf.advanceLine();
	}

	return obj;
}

void Server::loadSavePredec(GSFileParser & gsf)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(strtag.c_str());
		if (cls != -1) {
			std::string bpname = gsf.nextString(true);
			GameObjBlueprint *bp = gameSet->findBlueprint(cls, bpname);
			int numObjs = gsf.nextInt();
			int i = 0;
			while (i < numObjs) {
				int soi = gsf.nextInt();
				if (!soi) {
					gsf.advanceLine(); continue;
				}
				predec[soi] = bp;
				i++;
			}
		}
		else if (strtag == "END_PREDEC")
			break;
		gsf.advanceLine();
	}
}

void ServerGameObject::setItem(int index, float value)
{
	items[index] = value;

	NetPacketWriter msg(NETCLIMSG_OBJECT_ITEM_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(index);
	msg.writeFloat(value);
	Server::instance->sendToAll(msg);

}

void ServerGameObject::setParent(ServerGameObject * newParent)
{
	if (this->parent) {
		auto &vec = this->parent->children[this->blueprint->getFullId()];
		vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end()); // is order important?
	}
	this->parent = newParent;
	if(newParent)
		newParent->children[this->blueprint->getFullId()].push_back(this);

	NetPacketWriter msg(NETCLIMSG_OBJECT_PARENT_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(newParent->id);
	Server::instance->sendToAll(msg);
	
}

void ServerGameObject::setPosition(const Vector3 & position)
{
	this->position = position;

	NetPacketWriter msg(NETCLIMSG_OBJECT_POSITION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(position);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setOrientation(const Vector3 & orientation)
{
	this->orientation = orientation;

	NetPacketWriter msg(NETCLIMSG_OBJECT_ORIENTATION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(orientation);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setSubtypeAndAppearance(int new_subtype, int new_appearance)
{
	this->subtype = new_subtype;
	this->appearance = new_appearance;

	NetPacketWriter msg(NETCLIMSG_OBJECT_SUBTYPE_APPEARANCE_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(new_subtype);
	msg.writeUint32(new_appearance);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setColor(int color)
{
	this->color = color;
	NetPacketWriter msg(NETCLIMSG_OBJECT_COLOR_SET);
	msg.writeUint32(this->id);
	msg.writeUint8(color);
	Server::instance->sendToAll(msg);
}

void Server::sendToAll(const NetPacket & packet)
{
	for (NetLink *cli : clientLinks)
		cli->send(packet);
}

void Server::sendToAll(const NetPacketWriter & packet)
{
	for (NetLink *cli : clientLinks)
		cli->send(packet);
}

void Server::tick()
{
	timeManager.tick();
	auto &it = delayedSequences.begin();
	for (; it != delayedSequences.end(); it++) {
		if (it->first > timeManager.currentTime) {
			break;
		}
		DelayedSequence &ds = it->second;
		for (ServerGameObject *obj : ds.selfs) {
			ds.actionSequence->run(obj);
		}
	}
	delayedSequences.erase(delayedSequences.begin(), it);

	for (NetLink *cli : clientLinks) {
		int pcnt = 20;
		while (cli->available() && (pcnt--)) { // DDoS !!!
			NetPacket packet = cli->receive();
			BinaryReader br(packet.data.c_str());
			uint8_t type = br.readUint8();

			switch (type) {
			case NETSRVMSG_TEST: {
				std::string msg = br.readStringZ();
				chatMessages.push_back(msg);
				printf("Server got message: %s\n", msg.c_str());

				NetPacketWriter echo(NETCLIMSG_TEST);
				echo.writeStringZ(msg);
				sendToAll(echo);
				break;
			}
			case NETSRVMSG_COMMAND: {
				int cmdid = br.readUint32();
				int objid = br.readUint32();
				gameSet->commands[cmdid].execute(findObject(objid));
				break;
			}
			case NETSRVMSG_PAUSE: {
				uint8_t newPaused = br.readUint8();
				if (newPaused)
					timeManager.pause();
				else
					timeManager.unpause();
				break;
			}
			}
		}
	}
}

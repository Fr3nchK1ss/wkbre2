#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"

struct FinderUnknown : ObjectFinder {
	std::vector<ServerGameObject*> eval(ServerGameObject *self) {
		ferr("Unknown object finder called!");
		return std::vector<ServerGameObject*>();
	}
};

struct FinderSelf : ObjectFinder {
	std::vector<ServerGameObject*> eval(ServerGameObject *self) {
		return std::vector<ServerGameObject*>(1, self);
	}
};

struct FinderSpecificId : ObjectFinder {
	uint32_t objid;
	std::vector<ServerGameObject*> eval(ServerGameObject *self) {
		return std::vector<ServerGameObject*>(1, Server::instance->findObject(objid));
	}
	FinderSpecificId(uint32_t objid) : objid(objid) {}
};

struct FinderPlayer : ObjectFinder {
	std::vector<ServerGameObject*> eval(ServerGameObject *self) {
		return { self->getPlayer() };
	}
};

struct FinderAlias : ObjectFinder {
	int aliasIndex;
	std::vector<ServerGameObject*> eval(ServerGameObject *self) {
		auto &alias = Server::instance->aliases[aliasIndex];
		std::vector<ServerGameObject*> res;
		for (auto &ref : alias)
			if (ref)
				res.push_back(ref);
		return res;
	}
	FinderAlias(int aliasIndex) : aliasIndex(aliasIndex) {}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs)
{
	std::string strtag = gsf.nextString();
	if (strtag.substr(0, 7) != "FINDER_")
		return new FinderUnknown();
	std::string findername = strtag.substr(7);
	switch (Tags::FINDER_tagDict.getTagID(findername.c_str())) {
	case Tags::FINDER_SELF:
		return new FinderSelf();
	case Tags::FINDER_SPECIFIC_ID:
		return new FinderSpecificId(gsf.nextInt());
	case Tags::FINDER_PLAYER:
		return new FinderPlayer;
	case Tags::FINDER_ALIAS: {
		int ax = gs.aliases.readIndex(gsf);
		return new FinderAlias(ax);
	}
	}
	return new FinderUnknown();
}

#pragma once

#include "actions.h"

struct GameObjBlueprint;
struct ObjectFinder;
struct PositionDeterminer;
struct GSFileParser;
struct GameSet;

struct ObjectCreation {
	GameObjBlueprint *typeToCreate;
	ObjectFinder *controller;
	PositionDeterminer *createAt;
	ActionSequence postCreationSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
	void run(ServerGameObject *creator);
};
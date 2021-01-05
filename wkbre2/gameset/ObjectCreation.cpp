#include "ObjectCreation.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "finder.h"
#include "position.h"
#include "../server.h"
#include "ScriptContext.h"

void ObjectCreation::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "TYPE_TO_CREATE") {
			typeToCreate = gs.readObjBlueprintPtr(gsf);
		}
		else if (tag == "CONTROLLER") {
			controller = ReadFinder(gsf, gs);
		}
		else if (tag == "CREATE_AT") {
			createAt = PositionDeterminer::createFrom(gsf, gs);
		}
		else if (tag == "POST_CREATION_SEQUENCE") {
			postCreationSequence.init(gsf, gs, "END_POST_CREATION_SEQUENCE");
		}
		else if (tag == "END_OBJECT_CREATION")
			break;
		gsf.advanceLine();
	}
}

void ObjectCreation::run(ServerGameObject * creator)
{
	ServerGameObject *created = Server::instance->createObject(typeToCreate);
	auto _ = SrvScriptContext::creator.change(creator);
	created->setParent(controller->getFirst(creator));
	OrientedPosition opos = createAt ? createAt->eval(creator) : OrientedPosition({ creator->position, creator->orientation });
	created->setPosition(opos.position);
	created->setOrientation(opos.rotation);
	postCreationSequence.run(created);
}

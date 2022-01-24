// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Plan.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"

struct PFNUnknown : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	std::string name;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		ferr("Unknown plan node %s", name.c_str());
		return true;
	}
};

struct PFNAction : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	Action* action;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		action = ReadAction(gsf, gs);
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		action->run(ctx);
		return true;
	}
};

struct PFNWaitFor : PlanNodeBlueprint {
	struct State : PlanNodeState {
		float endTime;
	};
	std::unique_ptr<ValueDeterminer> waitTime;
	
	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		waitTime.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
		((State*)state)->endTime = Server::instance->timeManager.currentTime;
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		return Server::instance->timeManager.currentTime >= (((State*)state)->endTime + waitTime->eval(ctx));
	}
};

struct PFNWaitUntil : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	std::unique_ptr<ValueDeterminer> value;
	
	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		return value->booleval(ctx);
	}
};

struct PFNStartLoop : PlanNodeBlueprint {
	struct State : PlanNodeState {
		PlanNodeSequence::State seqState;
	};
	PlanNodeSequence sequence;
	std::unique_ptr<ValueDeterminer> loopBreakCondition;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		sequence.parse(gsf, gs, "END_LOOP");
		if (gsf.nextString() == "IF")
			loopBreakCondition.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		State* s = new State;
		s->seqState = sequence.createState();
		return s;
	}
	virtual void reset(PlanNodeState* state) override {
		sequence.reset(&((State*)state)->seqState);
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		bool done = sequence.execute(&((State*)state)->seqState, ctx);
		if (done) {
			if (loopBreakCondition && loopBreakCondition->booleval(ctx))
				return true;
			reset(state);
			return false;
		}
		else
			return false;
	}
};

PlanNodeBlueprint* PlanNodeBlueprint::createFrom(const std::string& tag, GSFileParser& gsf, const GameSet& gs)
{
	PlanNodeBlueprint* node;
	if (tag == "ACTION")
		node = new PFNAction;
	else if (tag == "WAIT_FOR")
		node = new PFNWaitFor;
	else if (tag == "WAIT_UNTIL")
		node = new PFNWaitUntil;
	else if (tag == "START_LOOP")
		node = new PFNStartLoop;
	else {
		auto unk = new PFNUnknown;
		unk->name = tag;
		node = unk;
	}
	node->parse(gsf, gs);
	return node;
}

void PlanNodeSequence::parse(GSFileParser& gsf, const GameSet& gs, const char* endtag)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		if (strtag == endtag)
			return;
		else if (!strtag.empty() && strtag.substr(0,2) != "//")
			nodes.emplace_back(PlanNodeBlueprint::createFrom(strtag, gsf, gs));
		gsf.advanceLine();
	}
	ferr("Plan node sequence reached end of file without %s!", endtag);
}

PlanNodeSequence::State PlanNodeSequence::createState()
{
	State s;
	for (auto& node : nodes)
		s.nodeStates.push_back(node->createState());
	return s;
}

void PlanNodeSequence::reset(State* state)
{
	state->currentNode = 0;
	nodes[0]->reset(state->nodeStates[0]);
}

bool PlanNodeSequence::execute(State* state, SrvScriptContext* ctx)
{
	while (state->currentNode < nodes.size()) {
		bool done = nodes[state->currentNode]->execute(state->nodeStates[state->currentNode], ctx);
		if (!done)
			break;
		state->currentNode += 1;
		if (state->currentNode < nodes.size())
			nodes[state->currentNode]->reset(state->nodeStates[state->currentNode]);
	}
	return state->currentNode >= nodes.size();
}

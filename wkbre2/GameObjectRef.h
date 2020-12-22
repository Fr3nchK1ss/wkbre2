#pragma once

#include <cstdint>
#include <functional>

template<typename Program, typename AnyGameObject> struct GameObjectRef {
	static const uint32_t NULL_GOREF = 0;

	uint32_t objid;

	AnyGameObject *get() const { if (objid != NULL_GOREF) return Program::instance->findObject(objid); else return nullptr; }
	AnyGameObject *operator->() const { return get(); }
	explicit operator bool() const { return get() != nullptr; }
	operator AnyGameObject *() const { return get(); }

	void set(AnyGameObject *obj) { if (obj) objid = obj->id; else objid = NULL_GOREF; }
	void set(uint32_t id) { objid = id; }
	GameObjectRef &operator=(AnyGameObject *obj) { set(obj); return *this; }
	GameObjectRef &operator=(uint32_t id) { set(id); return *this; }

	bool operator<(const GameObjectRef &other) const { return objid < other.objid; }
	bool operator==(const GameObjectRef &other) const { return objid == other.objid; }

	GameObjectRef() { objid = NULL_GOREF; }
	GameObjectRef(AnyGameObject *obj) { set(obj); }
	GameObjectRef(uint32_t id) { set(id); }
	GameObjectRef(const GameObjectRef &other) { objid = other.objid; }
	GameObjectRef(GameObjectRef &&other) { objid = other.objid; }
};

template<typename S, typename T> struct std::hash<GameObjectRef<S, T>> {
	size_t operator()(const GameObjectRef<S, T> &ref) const { return ref.objid; }
};

struct Server;
struct Client;
struct ServerGameObject;
struct ClientGameObject;

typedef GameObjectRef<Server, ServerGameObject> SrvGORef;
typedef GameObjectRef<Client, ClientGameObject> CliGORef;

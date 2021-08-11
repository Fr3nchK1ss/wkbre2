#pragma once

#include "util/vecmat.h"

struct Trajectory {
public:
	const float gravity = -9.81f;

	Vector3 getPosition(float time);
	void start(const Vector3& initPos, const Vector3& initVel, float startTime);
	bool isMoving() { return m_started; }

private:
	bool m_started = false;
	Vector3 m_initialPosition;
	Vector3 m_initialVelocity;
	float m_startTime;
};
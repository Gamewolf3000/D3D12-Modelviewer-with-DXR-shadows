#pragma once

#include <array>
#include <type_traits>
#include <functional>

#include "FrameBased.h"

template<typename T, FrameType Frames>
class FrameObject : public FrameBased<Frames>
{
protected:
	std::array<T, Frames> frameObjects;

public:
	FrameObject() = default;
	virtual ~FrameObject() = default;

	FrameObject(const FrameObject& other) = delete;
	FrameObject& operator=(const FrameObject& other) = delete;
	FrameObject(FrameObject&& other) = default;
	FrameObject& operator=(FrameObject&& other) = default;

	T& Active();
	T& Next();
	T& Last();

	void Initialize(const std::function<void(FrameType, T&)> initFunc);
	void Initialize(const std::function<void(T&)> initFunc);

	template<typename... InitializationTypes>
	void Initialize(void(T::*func)(InitializationTypes...),
		InitializationTypes... initializationArgument);
};

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Active()
{
	return frameObjects[this->activeFrame];
}

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Next()
{
	std::uint8_t nextFrame = (this->activeFrame + 1 == Frames) ? 0 : 
		this->activeFrame + 1;
	return frameObjects[nextFrame];
}

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Last()
{
	std::uint8_t lastFrame = (this->activeFrame == 0) ? Frames - 1 : 
		this->activeFrame - 1;
	return frameObjects[lastFrame];
}

template<typename T, FrameType Frames>
inline void FrameObject<T, Frames>::Initialize(
	const std::function<void(FrameType, T&)> initFunc)
{
	for (FrameType i = 0; i < Frames; ++i)
		initFunc(i, frameObjects[i]);
}

template<typename T, FrameType Frames>
inline void FrameObject<T, Frames>::Initialize(
	const std::function<void(T&)> initFunc)
{
	for (FrameType i = 0; i < Frames; ++i)
		initFunc(frameObjects[i]);
}

template<typename T, FrameType Frames>
template<typename ...InitializationTypes>
inline void FrameObject<T, Frames>::Initialize(
	void(T::*func)(InitializationTypes...),
	InitializationTypes... initializationArgument)
{
	for (auto& object : frameObjects)
		(object.*(func))(initializationArgument...);
}
#pragma once
#include <sstream>
#include <mutex>
#include "Clip.h"
#include "ISerializable.h"
class ClipsCollection : public ISerializable
{
public:
	ClipsCollection() noexcept;
	~ClipsCollection();

	std::string								Serialize() override;
	void									Deserialize(std::string serializationData) override;

	void									AddFront(std::shared_ptr<Clip> clip);
	void									RemoveAllOfFormat(DWORD format);
	std::shared_ptr<Clip>					RemoveAt(unsigned int index);
	std::shared_ptr<Clip>					At(size_t index);
	std::shared_ptr<Clip>					Front();
	unsigned int							Size() noexcept;
	unsigned int							MaxSize() noexcept;
	void									SetMaxSize(unsigned int maxSize);
	void									Clear();
	void									PurgeThumbnails();
	void									LoadAllResources();

private:
	// Guards clips and maxSize: the clips writer thread serializes the
	// collection while the UI thread modifies it.
	std::mutex								clipsMutex;
	std::deque<std::shared_ptr<Clip>>		clips;
	unsigned int							maxSize = 200;

	std::deque<std::shared_ptr<Clip>>		Snapshot();
	void									TrimToMaxSize();
};


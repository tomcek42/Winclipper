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
	bool									TogglePinAt(unsigned int index);
	unsigned int							PinnedCount();
	std::shared_ptr<Clip>					At(size_t index);
	std::shared_ptr<Clip>					Front();
	unsigned int							Size() noexcept;
	unsigned int							MaxSize() noexcept;
	void									SetMaxSize(unsigned int maxSize);
	void									Clear(bool includePinned);
	void									PurgeThumbnails();
	void									LoadAllResources();
	void									NormalizePinOrder();

private:
	// Guards clips and maxSize: the clips writer thread serializes the
	// collection while the UI thread modifies it.
	std::mutex								clipsMutex;
	// Invariant: pinned clips form a block at the front, in pin order.
	std::deque<std::shared_ptr<Clip>>		clips;
	unsigned int							maxSize = 200;

	std::deque<std::shared_ptr<Clip>>		Snapshot();
	void									TrimToMaxSize();
	size_t									PinnedCountLocked() const;
};


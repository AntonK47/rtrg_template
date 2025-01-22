#include "StreamingSystem.hpp"
#include "Profiler.hpp"

using namespace Framework;

void Framework::StreamingSystem::Initialize()
{
	streamingThread = std::make_unique<std::jthread>(&StreamingSystem::StreamingWorker, this);
}

void Framework::StreamingSystem::Deinitialize()
{
}

void Framework::StreamingSystem::Request(StreamingTask&& task)
{
	streamingTasks.push(task);
	hasAnyWork.notify_one();
}

void Framework::StreamingSystem::StreamingWorker(void* self)
{
#ifdef TRACY_ENABLE
	tracy::SetThreadName("Streaming Worker");
#endif
	auto& streamingSystem = *reinterpret_cast<StreamingSystem*>(self);

	while (true)
	{
		auto l = std::unique_lock(streamingSystem.lock);
		streamingSystem.hasAnyWork.wait(l, [&] { return not streamingSystem.streamingTasks.empty(); });

		StreamingTask task;
		const auto hasTaskAcquired = streamingSystem.streamingTasks.try_pop(task);

		if (hasTaskAcquired)
		{
			ZoneScopedN("Streaming Task");
			task.task();
		}


		l.unlock();
	}
}

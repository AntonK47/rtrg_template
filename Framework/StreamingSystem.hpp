#pragma once

#include "Core.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <rigtorp/MPMCQueue.h>
#include <thread>

namespace Framework
{

	struct StreamingSystem
	{
		struct StreamingTask
		{
			StreamingTask(std::function<void(void)> t) noexcept : task{t} 
			{
			}
			StreamingTask(){}

			StreamingTask(StreamingTask&& other) noexcept = default;
			StreamingTask(const StreamingTask&) noexcept = default;

			StreamingTask& operator =(StreamingTask& b) noexcept = default;
			StreamingTask& operator =(StreamingTask&& b) noexcept = default;


			std::function<void(void)> task;
		};
		void Initialize();
		void Deinitialize();

		void Request(StreamingTask&& task);


	private:
		static void StreamingWorker(void* self);

		rigtorp::MPMCQueue<StreamingTask> streamingTasks{ 1024 };
		std::condition_variable hasAnyWork;

		std::unique_ptr<std::jthread> streamingThread;

		mutable std::mutex lock;
	};
} // namespace Framework
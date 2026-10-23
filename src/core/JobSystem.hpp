#ifndef JOB_SYSTEM_HPP
#define JOB_SYSTEM_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace enki { class TaskScheduler; }

enum class JobPriority { High, Normal, Low };

// Обертка над enkits. init, sht, submit, collectfinished - только из главного потока
class JobSystem {
public:
	JobSystem();
	~JobSystem();
	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	// 0 - по числу ядер
	void Init(uint32_t workerThreads = 0);

	// Отмена неначатых задач + ожидание тех что еще в работе
	void Shutdown();

	void Submit(std::function<void()> job, JobPriority priority = JobPriority::Normal);

	void ParallelFor(uint32_t count, uint32_t minRange, const std::function<void(uint32_t begin, uint32_t end)>& fn);

	// Раз в кадр в главном потоке удалить объекты выполненных задач
	void CollectFinished();

	uint32_t GetThreadCount() const;
	std::size_t GetJobsInFlight() const { return m_jobs.size(); }	// еще не убранные задачи
	bool IsShuttingDown() const { return m_cancel.load(); }
private:
	struct Job;	// в cpp

	std::unique_ptr<enki::TaskScheduler> m_scheduler;
	std::vector<std::unique_ptr<Job>> m_jobs;	// для fire-and-forget
	std::atomic<bool> m_cancel{ false };	// true - новые и неначатые задачи не выполняются
	// atomic нужен, потому что его ставит главный поток, а читают рабочие потоки одновременно

	std::thread::id m_mainThread;
	bool m_initialized = false;
};

#endif
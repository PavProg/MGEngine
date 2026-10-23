#include "JobSystem.hpp"
#include "logger/logger.hpp"
#include "core/profiler.hpp"

#include "enkiTS/TaskScheduler.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <string>

namespace {
	enki::TaskPriority ToEnki(JobPriority p) {
		switch (p)
		{
		case JobPriority::High:
			return enki::TASK_PRIORITY_HIGH;
		case JobPriority::Low:
			return enki::TASK_PRIORITY_LOW;
		default:
			return enki::TASK_PRIORITY_MED;
		}
	}

	// enkits вызовет в начале каждого потока и даст имя для трейси
	void OnWorkerStart(uint32_t threadnum) {
		std::string name = "Job Worker " + std::to_string(threadnum);
		MGE_PROFILE_THREAD(name.c_str());
	}

	// исключение внутри задачи не роняет весь процесс
	template<typename Fn>
	void RunGuarded(Fn&& fn) {
		try {
			fn();
		}
		catch (const std::exception& e) {
			Logger::Error(std::string("JobSystem: job threw exception: ") + e.what());
		}
		catch (...) {
			Logger::Error(std::string("JobSystem: job threw unknown exception"));
		}
	}
}

// описание задачи в заголовке
struct JobSystem::Job : enki::ITaskSet {
	std::function<void()> fn;
	const std::atomic<bool>* cancel = nullptr;

	void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
		if (cancel->load()) return;
		MGE_PROFILE_SCOPE_N("Job");
		RunGuarded(fn);
	}
};

// конструктор не запускает планировщик!
JobSystem::JobSystem() : m_scheduler(std::make_unique<enki::TaskScheduler>()) {}

JobSystem::~JobSystem() {
	Shutdown();
}

void JobSystem::Init(uint32_t workerThreads) {
	assert(!m_initialized && "Job system: Init called twice");

	// поток вызвавший Init становится главным
	m_mainThread = std::this_thread::get_id();

	enki::TaskSchedulerConfig config;
	if (workerThreads > 0) config.numTaskThreadsToCreate = workerThreads;
	config.profilerCallbacks.threadStart = &OnWorkerStart;

	m_scheduler->Initialize(config);
	m_cancel = false;
	m_initialized = true;

	Logger::Info("JobSystem: " + std::to_string(GetThreadCount() - 1) + " worker threads + main thread");
}

void JobSystem::Shutdown() {
	if (!m_initialized) return;
	MGE_PROFILE_SCOPE_N("JobSystemShutdown");

	m_cancel = true;	// неначатые задачи обнуляются
	m_scheduler->WaitforAllAndShutdown(); // дождаться остальных
	m_jobs.clear();		// все задачи завершены - можно удалять
	m_initialized = false;

	Logger::Info("JobSystem: shut down");
}

void JobSystem::Submit(std::function<void()> fn, JobPriority priority) {
	assert(std::this_thread::get_id() == m_mainThread && "JobSystem::Submit only from main thread");

	if (m_cancel) return;
	if (!m_initialized) {
		RunGuarded(fn);
		return;
	}

	auto job = std::make_unique<Job>();
	job->fn = std::move(fn);
	job->cancel = &m_cancel;
	job->m_Priority = ToEnki(priority);

	m_scheduler->AddTaskSetToPipe(job.get());	// отдать задачу enkits, get получить обычный указатель
	m_jobs.push_back(std::move(job));
}

void JobSystem::ParallelFor(uint32_t count, uint32_t minRange, const std::function<void(uint32_t, uint32_t)>& fn) {
	if (count == 0) return;
	if (!m_initialized) {
		RunGuarded([&] { fn(0, count); });
		return;
	}

	MGE_PROFILE_SCOPE_N("ParallelFor");
	enki::TaskSet task(count, [&fn](enki::TaskSetPartition range, uint32_t) {	// набор задач с лямдой
		MGE_PROFILE_SCOPE_N("ParallelForRange");
		RunGuarded([&] { fn(range.start, range.end); });
	});
	task.m_MinRange = std::max<uint32_t>(1, minRange);

	m_scheduler->AddTaskSetToPipe(&task);
	m_scheduler->WaitforTask(&task);
}

// задачи fire-and-forget лежат в списке и после выполнения. чистим GetIsComplete()
void JobSystem::CollectFinished() {
	m_jobs.erase(std::remove_if(m_jobs.begin(), m_jobs.end(),
		[](const std::unique_ptr<Job>& job) { return job->GetIsComplete(); }),
		m_jobs.end());
}

uint32_t JobSystem::GetThreadCount() const {
	return m_initialized ? m_scheduler->GetNumTaskThreads() : 1;
}
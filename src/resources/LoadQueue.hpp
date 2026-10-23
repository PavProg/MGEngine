#ifndef LOAD_QUEUE_HPP
#define LOAD_QUEUE_HPP

#include <chrono>
#include <functional>
#include <queue>
#include <mutex>

#include "core/profiler.hpp"
#include "core/JobSystem.hpp"

// Тяжелая часть работы уходит в джобы. Легкая с OpenGL (финализатор) ждет здесь, пока главный поток не закончит в Pump
class LoadQueue {
public:
	void SetJobSystem(JobSystem* jobs) { m_jobs = jobs;  }
	
	void Submit(std::function<void()> job, JobPriority priority = JobPriority::Normal) {
		if (m_jobs) m_jobs->Submit(std::move(job), priority);
		else job();
	}

	void PushFinalize(std::function<void()> fin) {
		std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
		m_finals.push(std::move(fin));
	}

	// Главный поток. Выполняет финализаторы пока не кончится бюджет времени
	void Pump(double budgetMs) {
		using Clock = std::chrono::steady_clock;
		const auto start = Clock::now();
		for (;;) {
			std::function<void()> fin;
			{
				std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
				if (m_finals.empty()) return;
				fin = std::move(m_finals.front());
				m_finals.pop();
			}
			fin();

			const double elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
			if (elapsedMs >= budgetMs) return;
		}
	}

	// Выбросить невыполненные финализаторы
	void DropPendingFinalizers() {
		std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
		std::queue<std::function<void()>>().swap(m_finals);	// меняем старую очередь на новую пустую, проще чистить.
	}

	// Для графика в трейси. Сколько готовых результатов ждут выгрузки на gpu
	std::size_t PendingFinalizers() {
		std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
		return m_finals.size();
	}



private:
	JobSystem* m_jobs = nullptr;
	std::queue<std::function<void()>> m_finals;
	MGE_PROFILE_LOCKABLE(std::mutex, m_finMtx);
};

#endif
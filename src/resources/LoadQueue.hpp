#ifndef LOAD_QUEUE_HPP
#define LOAD_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

#include "core/profiler.hpp"
#include <string>

class LoadQueue {
public:
	void Start(int workers = 2) {
		m_running = true;
		for (int i = 0; i < workers; ++i)
			m_threads.emplace_back([this, i] {
				std::string name = "LoadWorker " + std::to_string(i);
				MGE_PROFILE_THREAD(name.c_str());
				WorkerLoop(); 
			});
	}
	void Stop() {
		{ std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_jobMtx); m_running = false; }
		m_jobCv.notify_all();
		for (auto& t : m_threads) if (t.joinable()) t.join();
		m_threads.clear();
	}

	void Submit(std::function<void()> job) {
		{ std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_jobMtx); m_jobs.push(std::move(job)); }
		m_jobCv.notify_one();
	}

	void PushFinalize(std::function<void()> fin) {
		std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
		m_finals.push(std::move(fin));
	}

	void Pump(int maxPerFrame = 4) {
		for (int i = 0; i < maxPerFrame; ++i) {
			std::function<void()> fin;
			{
				std::lock_guard<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_finMtx);
				if (m_finals.empty()) return;
				fin = std::move(m_finals.front());
				m_finals.pop();
			}
			fin();
		}
	}
private:
	void WorkerLoop() {
		for (;;) {
			std::function<void()> job;
			{
				std::unique_lock<MGE_PROFILE_LOCK_TYPE(std::mutex)> lk(m_jobMtx);
				m_jobCv.wait(lk, [this]{ return !m_jobs.empty() || !m_running; });
				if (!m_running && m_jobs.empty()) return;
				job = std::move(m_jobs.front());
				m_jobs.pop();
			}
			job();
		}
	}

	std::vector<std::thread> m_threads;
	std::queue<std::function<void()>> m_jobs;
	std::queue<std::function<void()>> m_finals;
	MGE_PROFILE_LOCKABLE(std::mutex, m_jobMtx);
	MGE_PROFILE_LOCKABLE(std::mutex, m_finMtx);
	std::condition_variable_any m_jobCv;
	std::atomic<bool> m_running{ false };
};

#endif
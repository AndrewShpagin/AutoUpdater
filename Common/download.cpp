#include "download.h"
#include "zpp.h"

namespace downloader {
	queue::queue(int max_threads, int retry_attempts, std::function<bool(size_t, size_t)> progress) {
		_max_threads = max_threads;
		_retry_attempts = retry_attempts;
		endall = false;
		allOk = true;
		allSize = 0;
		allDownloaded = 0;
		_progress = std::move(progress);
	}

	void queue::waitTheFinish() {
		m.lock();
		endall = true;
		std::vector<std::thread*> temp = threads;
		m.unlock();
		for (size_t i = 0; i < temp.size(); i++) {
			temp[i]->join();
		}
		m.lock();
		for (size_t i = 0; i < threads.size(); i++) delete(threads[i]);
		for (size_t i = 0; i < dqueue.size(); i++) delete(dqueue[i]);
		threads.clear();
		dqueue.clear();
		m.unlock();
	}

	queue::~queue() {
		waitTheFinish();
	}

	bool queue::success() {
		return allOk;
	}

	void queue::add(const std::string& url, const std::string& to, bool unzip, std::function<void()> ready,
	                std::function<void(const std::string&)> error) {
		if (!endall) {
			std::scoped_lock lock(m);

			element* de = new element;
			de->URL = url;
			de->final_pos = to;
			de->needUnzip = unzip;
			de->size = 0;
			de->downloadedSize = 0;
			de->output = nullptr;
			de->attempts = 0;
			de->remove = false;
			de->ready = std::move(ready);
			de->error = std::move(error);

			dqueue.push_back(de);

			if (threads.size() < _max_threads) {
				threads.push_back(new std::thread(
					[this] {
						do {
							element* de = nullptr;
							m.lock();
							static size_t didx = rand();
							for (size_t i = 0; i < dqueue.size(); i++) {
								if (dqueue[i]->output == nullptr) {
									de = dqueue[i];
									break;
								}
							}
							if (de) {
								de->temporary_pos = de->final_pos + ".download." + std::to_string(didx++);
								jcc::createPath(de->temporary_pos);
								de->output = new std::ofstream(de->temporary_pos, std::ios::binary);
								de->remove = true;
							}
							m.unlock();
							if (de && de->output) {
								std::string server;
								std::string com;
								bool serv = true;
								for (auto c : de->URL) {
									if (serv && (c == '/' || c == '\\' || c == '?') && server != "https:" && server !=
										"http:" && server != "https:/" && server != "http:/")serv = false;
									if (serv)server += c;
									else com += c;
								}
								bool success = false;
								if (server.length() && de->output && de->output->is_open()) {
									httplib::Client cli(server);
									auto res = cli.Get(com.c_str(),
									                   [&](const char* data, size_t data_length)-> bool {
										                   de->output->write(data, data_length);
										                   return true;
									                   },
									                   [&](uint64_t current, uint64_t total)-> bool {
										                   bool ret = true;
										                   m.lock();
										                   allSize += total - de->size;
										                   allDownloaded += current - de->downloadedSize;
										                   if (_progress) {
											                   ret = _progress(allDownloaded, allSize);
										                   }
										                   m.unlock();
										                   de->downloadedSize = current;
										                   de->size = total;
										                   return ret;
									                   });
									de->output->close();
									delete(de->output);

									std::filesystem::directory_entry dest(de->final_pos);
									std::filesystem::directory_entry tmp(de->temporary_pos);

									if (res && res->status == 200) {
										if (dest.exists()) {
											std::error_code ec;
											remove(dest, ec);
											if (ec.value()) {
												allOk = false;
												std::cout << "Unable to remove file: " << dest << ", error: " << ec.
													message() << "\n";
											}
										}
										if (de->needUnzip) {
											{
												zpp::reader w(de->temporary_pos);
												w.extractFirstToFile(de->final_pos);
											}
											if (tmp.exists()) {
												std::error_code ec;
												remove(tmp, ec);
												if (ec.value()) {
													allOk = false;
													std::cout << "Unable to remove file: " << ec.message() << "\n";
												}
											}
											if (dest.exists()) {
												success = true;
												if (de->ready)de->ready();
											}
											else {
												allOk = false;
												std::cout << "Missing: " << de->final_pos << "\n";
												if (de->error)de->error("Unable to unzip");
											}
										}
										else {
											std::error_code co;
											rename(tmp, dest, co);
											if (co.value() == 0) {
												success = true;
												if (de->ready)de->ready();
											}
											else {
												allOk = false;
												std::cout << co.message() << "\n";
												if (de->error)de->error(co.message());
											}
										}
									}
									else {
										if (de->attempts < _retry_attempts) {
											m.lock();
											de->attempts++;
											std::this_thread::sleep_for(std::chrono::milliseconds(300));
											de->remove = false;
											m.unlock();
										}
										else {
											if (de->error)de->error("Download failed after all attempts");
										}
										allOk = false;
										std::cout << "Download failed [network failure]: " << de->URL <<
											" Re-trying, attempt #" << de->attempts << "\n";
									}
									std::error_code ec;
									remove(tmp, ec);
									if (ec.value()) {
										allOk = false;
										std::cout << "Unable to remove temporary file: " << dest << ", error: " << ec.
											message() << "\n";
										if (de->error)de->error(ec.message());
									}
								}
								else {
									allOk = false;
									std::cout << "Download failed [internal error]: " << de->URL << "\n";

								}
								m.lock();
								if (de->remove) {
									for (auto it = dqueue.begin(); it != dqueue.end(); ++it) {
										if (*it == de) {
											delete(de);
											dqueue.erase(it);
											break;
										}
									}
								}
								else de->output = nullptr;
								m.unlock();
							}
							else if (endall)break;
							std::this_thread::sleep_for(std::chrono::milliseconds(5));
						}
						while (TRUE);
					}));
			}
		}
	}

	size_t queue::getProgress() {
		size_t p = 0;
		m.lock();
		size_t sub = allSize >> 8;
		if (sub > 0) p = ((allDownloaded >> 8) * 100) / sub;
		m.unlock();
		return p ? p : 100;
	}

	std::pair<size_t, size_t> queue::getDownloadedSize() {
		m.lock();
		std::pair<size_t, size_t> p(allSize, allDownloaded);
		m.unlock();
		return p;
	}
};
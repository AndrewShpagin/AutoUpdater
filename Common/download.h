#pragma once
#include <ios>
#include <mutex>
#include <string>
#include <vector>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "jcc.h"

namespace downloader {
	/// The class for the asyncronous downloading. Create the object, add items to download and do other job or wait till all downloads will be finished.
	class queue {
	public:
		/**
		 * \brief Construct the download queue
		 * \param max_threads maximum dowloading threads
		 * \param retry_attempts retry attempts count
		 * \param progress the callback to follow the progress, called with (downloaded, size_to+download)
		 */
		queue(int max_threads = 2, int retry_attempts = 20, std::function<bool(size_t, size_t)> progress = nullptr);
		~queue();
		
		/// Whait till all downloads finish. You can't add new downloads after this command.
		void waitTheFinish();

		/// returns true if all downloads succeed.
		bool success();
		
		/**
		 * \brief Add the item to be downloaded.
		 * \param url the URL to download
		 * \param to the filename for the downloaded file
		 * \param unzip need unzip after the downloading?
		 * \param ready the ready callback
		 * \param error the errors callback, called with the error message.
		 */
		void add(const std::string& url, const std::string& to, bool unzip, std::function<void()> ready = nullptr,
		         std::function<void(const std::string&)> error = nullptr);

		/// returns the overall progress
		size_t getProgress();

		/// returns the downloaded size and the total size to be downloaded as the pair
		std::pair<size_t, size_t> getDownloadedSize();

	protected:
		struct element {
			std::string URL;
			std::string temporary_pos;
			std::string final_pos;
			std::ofstream* output;
			bool needUnzip;
			bool remove;
			size_t downloadedSize;
			size_t size;
			size_t attempts;
			std::function<void()> ready;
			std::function<void(const std::string&)> error;
		};
		std::mutex m;
		std::mutex unz;
		std::vector<element*> dqueue;
		std::vector<std::thread*> threads;
		std::function<bool(size_t, size_t)> _progress;
		size_t _max_threads;
		size_t _retry_attempts;
		size_t allSize;
		size_t allDownloaded;
		bool endall;
		bool allOk;
	};
};

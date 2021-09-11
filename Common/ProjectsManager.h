#pragma once
#include <string>
#include <vector>

#include "HeapFilesSync.h"
#include "json.h"


namespace gsproject {
	class Manager {
		fheap::FilesHeap heap;
		json::JSON image;
		std::string PathToFiles;
		std::vector<std::string> Exceptions;
		std::string HeapPath;
		std::string VersionFileRelativePath;
		std::string Server;
		std::string RemoteFilesPath;
		std::string Bucket;
		std::string VersionsList;
		bool SyncDown;
	public:
		Manager();
		Manager(const std::string& path);
		void readConfig(const std::string& path);
		bool createImage(bool upload, const std::string versionFileName = "");
		std::string gsutil(const std::string& params);
	};
}

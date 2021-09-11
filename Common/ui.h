#pragma once
#include "HeapFilesSync.h"

class installerUi : public fheap::FilesHeap {
	json::JSON image;
	json::JSON versions;
	std::vector<std::string> except;
	std::string _version;
	std::string _product;
	std::string exe;
public:
	installerUi(const std::string& heapPath, const std::string& installPath, const std::string& downloadUrl, const std::string& version, const std::string& product);
	installerUi();
	void setVersionsList(const json::JSON& list);
	void setImage(const json::JSON& im);
	void setExceptions(const json::JSON& exc);
	void setExe(const std::string& _exe);
	bool start();
};
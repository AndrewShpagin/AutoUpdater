#include "../Common/HeapFilesSync.h"
#include "../Common/jcc.h"
#include "../Common/ui.h"
#include "../Common/ProjectsManager.h"
#include "../Common/tools.h"

void shortcut(const char*, const char*);

void start() {	
	std::locale::global(std::locale("en_US.UTF-8"));
	size_t na = jcc::args().size();
	std::filesystem::path js = jcc::getexepath();
	std::cout << "Exe: " << js << "\n";
	js.remove_filename();
	js.append("AutoUpdater.json");
		
	json::JSON param;
	jcc::readJson(param, js.string());
	auto arg = [&](const std::string& name)->std::string {
		if (param.hasKey(name))return param[name].ToString();
		return "";
	};

	std::cout << "Parameters:\n" << param.dump() << "\n";
	
	std::string bucket = arg("Bucket");
	std::string heapPath = arg("HeapPath");
	std::string installPath = arg("PathToFiles");
	std::string version = arg("Version");
	std::string product = arg("Product");
	std::string menu = arg("StartMenu");
	std::string heapURL = arg("HeapURL");
	if(menu.length()) {
		shortcut(jcc::getexepath().string().c_str(), menu.c_str());
	}
	
	json::JSON exc = json::Array();
	if (param.hasKey("Exceptions"))exc = param["Exceptions"];
	if(bucket.length()) {
		heapURL = "https://storage.googleapis.com/" + bucket + "/heap/";
	}

	std::cout << "Heap path: " << heapPath << "\n";
	std::cout << "Install path: " << installPath << "\n";
	std::cout << "Heap path: " << heapPath << "\n";
	std::cout << "Heap URL: " << heapURL << "\n";
	std::cout << "Product: " << product << "\n";
	
	if (!heapPath.empty() && !installPath.empty() && !heapURL.empty() && ! product.empty()) {		
		installerUi ui(heapPath, installPath, heapURL, version, product);
		ui.setExceptions(exc);
		ui.setExe(arg("exe"));
		ui.start();
	}
}

#if defined _WIN32 && !defined _DEBUG
int  WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
) {		
	start();
}
#else
int main(int argc, char* argv[])
{
	std::locale::global(std::locale("en_US.UTF-8"));
	jcc::passArgs(argc, argv);
	start();
	return 0;
}
#endif


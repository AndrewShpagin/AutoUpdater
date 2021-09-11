#include "HeapFilesSync.h"
#include "jcc.h"
#include "ui.h"
#include "download.h"
#include "tools.h"

installerUi::installerUi(const std::string& heapPath, const std::string& installPath, const std::string& downloadUrl, const std::string& version, const std::string& product) {
	setHeapPlacement(heapPath);
	setDestinationFolder(installPath);	
	_version = version;
	_product = product;
	setServer("https://storage.googleapis.com", downloadUrl);
}

installerUi::installerUi() {

}

void installerUi::setVersionsList(const json::JSON& list) {
	
}

void installerUi::setImage(const json::JSON& im) {
	image = im;
}

void installerUi::setExceptions(const json::JSON& exceptions) {
	if (exceptions.size()) {
		for (int i = 0; i < exceptions.size(); i++) {
			std::string s = exceptions.at(i).ToString();
			if (!s.empty())except.push_back(s);
		}
	}
}

void installerUi::setExe(const std::string& _exe) {
	exe = _exe;
}

bool installerUi::start() {
	std::filesystem::path versionsPath = _heapPath;
	versionsPath.append("Versions/root.json");
	std::filesystem::path heapDat = _heapPath;
	heapDat.append("Versions/heap.dat");
	//re-download root.json
	{
		downloader::queue q(1,5);
		std::string str = _servpath;
		q.add(_servpath + "Versions/root.json", versionsPath.string(), false);
		q.add(_servpath + "Versions/heap.dat", heapDat.string(), true);
		q.waitTheFinish();
		jcc::readSafeJson(versions, versionsPath.string());
		if (versions.IsNull()) {
			/// unable to start because the root.json inaccessible
			return false;
		}
		_hashesList = jcc::readFile(heapDat.string());
	}
	int finished = 0;
	std::mutex m;
	std::string last_title;
	std::string progress_percent;
	bool shouldStop = false;
	std::string errMsg;

	setErrors(
		[&](const std::string& stage, const std::string& the_problem_to_display) {
			std::cout << "ERROR! " << stage << " : " << the_problem_to_display << "\n";
			errMsg = "ERROR: " + stage + " : " + the_problem_to_display;
		});
	auto report = [&](const std::string& title, int progress) {
		std::scoped_lock lk(m);
		progress_percent = std::to_string(progress);
		last_title = title;
	};
	auto ftostr = [](float x) -> std::string {
		return std::to_string(x).substr(0, std::to_string(x).find(".") + 2);
	};
	setProgress([&](size_t cur, size_t num, const std::string& file, const std::string& stage) {
		if (num && cur <= num) {			
			std::string title = stage;
			if (stage == "Estimating") {
				title = "Indexing & checking the current installation";
			}
			if (stage == "Downloading") {
				size_t tot = num >> 10;
				size_t dow = cur >> 10;
				title += " [" + ftostr(float(dow) / 1000) + "/" + ftostr(float(tot) / 1000) + " MB]";
			}
			if (stage == "Copying") {
				title = "UnZip & Copy:";
				size_t tot = num >> 10;
				size_t dow = cur >> 10;
				title += " [" + ftostr(float(dow) / 1000) + "/" + ftostr(float(tot) / 1000) + " MB]";
			}
			while (num > 256) {
				num >>= 1;
				cur >>= 1;
			}
			report(title, (int)(cur * 100 / num));
		}
		return !shouldStop;
	});
	/// sort versions list
	bool ch = false;
	do {
		ch = false;
		for (int i = 1; i < versions.size(); i++) {
			if (versions[i - 1]["Version"].ToString() < versions[i]["Version"].ToString()) {
				std::swap(versions[i - 1], versions[i]);
				ch = true;
			}
		}
	} while (ch);
	/// download missing versions images
	downloader::queue dq(4, 10);
	for (int i = 0; i < versions.size(); i++) {
		if (versions[i].hasKey("Version")) {
			std::string& set_version = versions[i]["Version"].ToString();
			std::filesystem::path version_path = _heapPath;
			version_path.append("Versions/" + _product + set_version + ".json");
			std::filesystem::directory_entry de(version_path);
			if (!de.exists()) {
				std::string str = _servpath;
				str += "Versions/" + _product + set_version;
				dq.add(str, version_path.string(), true);
			}
		}
	}

	jcc::LocalServer ls;
	jcc::Html h("installer.html", ls);
	h.Replace("['VERSIONS']", versions.dump());
	h.Replace("CURRENTVERSION", _version);
	h.Replace("PRODUCT", _product);
	h.Replace("INSTALLPATH", _dest.string());
	bool syncStarted = false;
	std::string syncedTo = "";

	ls.exchange(
		[&](const json::JSON& in)-> json::JSON {
			json::JSON res = json::Object();
			std::string req = in.dump();
			if (in.hasKey("request")) {
				if (in.at("request") == "progress") {
					size_t prog = dq.getProgress();
					if (prog < 100) {
						res["progress"] = std::to_string(prog);
						res["title"] = "Downloading the builds list";
					}
					else {
						std::scoped_lock lk(m);
						res["progress"] = progress_percent;
						res["title"] = last_title;
						if (finished) {
							res["finish"] = true;
							finished++;
						}
						if (finished > 3) {
							ls.signalToStop();
						}
					}
					if (syncedTo.length()) {
						res["syncedTo"] = syncedTo;
						syncedTo.clear();

					}
					if (!errMsg.empty()) {
						res["error"] = errMsg;
						res["progress"] = "0";
					}
				}
			}
			if (in.at("request") == "run") {
				if(!exe.empty()) {
					execute(exe, "");
				}
			}
			if (in.at("request") == "stop") {
				shouldStop = true;
			}
			if (in.at("request") == "setimage") {
				if (in.hasKey("Version") && !syncStarted) {
					progress_percent = "0";
					last_title = "Preparing...";
					errMsg = "";
					std::string& set_version = in.at("Version").ToString();
					std::filesystem::path version_path = _heapPath;
					version_path.append("Versions/" + _product + set_version + ".json");
					std::filesystem::directory_entry de(version_path);
					if (de.exists()) {
						json::JSON image;
						if (jcc::readSafeJson(image, version_path.string())) {
							shouldStop = false;
							syncStarted = true;							
							if (this->syncDestination(image, true, false, &except)) {
								syncedTo = set_version;
							} else {
								progress_percent = "0";															
							}
							syncStarted = false;
						}
					}					
				}
			}
			return res;
		});
	ls.open(h);
	std::thread t([&] {
		ls.listen();
	});
	dq.waitTheFinish();	
	//report(shouldStop? "Cancelled!" : "Finished!", 100);
	ls.wait();
	ls.stopGracefully();
	t.join();
	return true;
}

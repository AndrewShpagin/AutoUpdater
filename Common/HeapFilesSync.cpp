// HeapFilesSync.cpp : Defines the entry point for the application.
//

#include "HeapFilesSync.h"
#include "exec.h"
#include "jcc.h"
#include "download.h"

std::filesystem::path fheap::FilesHeap::temp_unique() {
	static int idx = 0;
	std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	idx++;
	std::filesystem::path p = _heapPath;
	std::string s = std::to_string(ms.count());
	s += "_" + std::to_string(idx);
	p.append(s);
	return p;
}

std::filesystem::path fheap::FilesHeap::_path(const std::string& hash) {
	std::filesystem::path p = _heapPath;
	p.append(hash.substr(0, 2));
	p.append(hash);
	return p;
}

bool fheap::FilesHeap::checkIntegrity(const std::filesystem::path& path, const std::string& hash,
                                             const std::string& ziphash) {
	if (ziphash.length() == 32) {
		return md5::file_hash(path.string()) == ziphash;
	}
	else {
		std::string p = temp_unique().string();
		zpp::reader zz(path.string());
		zz.extractFirstToFile(p);
		bool eq = md5::file_hash(p) == hash;
		std::filesystem::remove(p);
		return eq;
	}
}

std::string progress(size_t n, size_t m) {
	std::string r = "[";
	for (size_t k = 0; k < 40; k++) {
		if (k * m / 40 < n)r += "=";
		else r += ".";
	}
	r += "]";
	return r;
}
bool showProgress(size_t cur, size_t num, const std::string& file, const std::string& stage) {
	if (num && cur <= num) {
		std::string ex = "";
		if (stage == "Downloading") {
			size_t tot = num >> 10;
			size_t dow = cur >> 10;
			ex = "[" + std::to_string(float(dow) / 1000) + "/" + std::to_string(float(tot) / 1000) + " MB]";
		}
		while (num > 256) {
			num >>= 1;
			cur >>= 1;
		}
		size_t p = cur * 40 / num;
		static size_t pp = -1;
		if (p != pp) {
			std::cout << "\r" << stage << ": " << progress(cur, num) << ex << "                           ";
		}
		pp = p;
	}
	return true;
}

fheap::FilesHeap::FilesHeap() {
	progress = showProgress;
	setErrors(
		[](const std::string& stage, const std::string& the_problem_to_display) {
			std::cout << "ERROR! " << stage << " : " << the_problem_to_display << "\n";
		});
	log = false;
}

fheap::FilesHeap::~FilesHeap() {
	
}

void fheap::FilesHeap::setProgress(progressFn fn) {
	progress = fn;
}

void fheap::FilesHeap::setErrors(errorsFn fn) {
	errors = fn;
}

void fheap::FilesHeap::setHeapPlacement(const std::filesystem::path& path) {
	_heapPath = path;
	create_directories(path);
	if (!std::filesystem::exists(path)) {
		_heapPath.clear();
	}
}

void fheap::FilesHeap::setDestinationFolder(const std::filesystem::path& path) {
	_dest = path;
	create_directories(path);
	if(!std::filesystem::exists(path)) {
		_dest.clear();
	}
}

void fheap::FilesHeap::setServer(const std::string& server, const std::string& initialPath) {
	_server = server;
	_servpath = initialPath;
}

bool fheap::FilesHeap::syncDestination(const json::JSON& image, bool remove_extra_files, bool skipUserBreak, std::vector<std::string>* exceptions) {
	if (!valid())return false;
	if (log) std::cout << "\nSyncing the folder: " << _dest << "\n";
	json::JSON old;
	if (!createDestFolderImage(old, FALSE, true, exceptions))return false;
	const std::string md5 = "md5";
	const std::string zip = "zip";
	size_t total = 0;
	size_t cur = 0;
	bool err = false;
	bool userBreak = false;
	std::string stage = "Downloading";
	auto errorHandler = [&](const std::string& rec, std::filesystem::filesystem_error* e = nullptr) {
		if (errors) errors(stage, rec);
		if (e) std::cout << e->what();
		err = true;
	};
	/// list the files to download
	std::map<std::string,std::string> zhashes;
	std::map<std::string, std::string> fname;
	std::vector<std::string> hashes;
	downloader::queue dq(12, 20, [&](size_t c, size_t t) -> bool{
		cur = c;
		total = t;
		return progress(cur, total, "", "Downloading");
	});

	auto progressHandler = [&](const std::string& filename, const std::string& stage) -> bool {
		if (progress) {
			if (!progress(cur, total, filename, stage) && !skipUserBreak) {
				errorHandler("UserBreak");
				userBreak = true;
				return true;
			}
		}
		return false;
	};
	bool needDownload = false;
	for (auto [key, item] : image.ObjectRange()) {
		if (item.hasKey(md5)) {
			std::string filename = key;
			std::string hash = item[md5].ToString();
			std::string zhash = item.hasKey(zip) ? item[zip].ToString() : "";
			bool exists = false;
			if (old.hasKey(filename)) {
				const json::JSON& oi = old[filename];
				if (oi.hasKey(md5) && oi.at(md5) == hash) {
					exists = true;
				}
			}
			if (!exists && zhashes.count(hash) == 0) { /// the destination file does not exist
				if(!needDownload) {
					needDownload = true;
					if (progress)progress(0, 100, "", "Downloading...");
				}
				std::cout << "Need: " << key << "\n";
				std::filesystem::path dst = _path(hash);
				if (std::filesystem::exists(dst)) {
					/// the file with the hashsumm exists in the heap
					continue;
				}
				size_t fsize = 0;
				if (item.hasKey("size"))fsize = item["size"].ToInt();
				total += fsize;
				zhashes[hash] = zhash;
				fname[hash] = key;
				hashes.push_back(hash);
				std::filesystem::path temp = temp_unique();
				zpp::createPathForFile(temp.string());
				dq.add(_servpath + hash.substr(0, 2) + "/" + hash, temp.string(), false,
					[this, temp, hash, zhash, errorHandler] {
						if (checkIntegrity(temp, hash, zhash)) {
							try {
								std::filesystem::path dst = _path(hash);
								zpp::createPathForFile(dst.string());
								if (std::filesystem::exists(dst))remove(dst);
								std::filesystem::rename(temp, dst);
							}
							catch (std::filesystem::filesystem_error& e) {
								errorHandler("DataAccessError", &e);
							}
						}
						else {
							errorHandler("ServerDataCorrupted");
						}
						if (std::filesystem::exists(temp))remove(temp);
					},
					[errorHandler](const std::string& err) {
						errorHandler(err);
					});
			}
		}
	}
	dq.waitTheFinish();
	
	if (err)return false;
	/// now everything downloaded, ready to copy
	total = 0;
	cur = 0;
	for (auto [key, item] : image.ObjectRange()) {
		if (item.hasKey(md5)) {
			std::string hash = item[md5].ToString();
			std::string zhash = item.hasKey(zip) ? item[zip].ToString() : "";
			bool exists = false;
			if (old.hasKey(key)) {
				const json::JSON& oi = old[key];
				if (oi.hasKey(md5) && oi.at(md5) == hash) {
					exists = true;
				}
			}
			if (!exists && item.hasKey("size")) {
				total += item["size"].ToInt();
			}
		}
	}
	stage = "Copy files";
	bool anyCopy = false;
	bool anyFail = false;
	for (auto [key, item] : image.ObjectRange()) {
		if (item.hasKey(md5)) {
			std::string hash = item[md5].ToString();
			std::string zhash = item.hasKey(zip) ? item[zip].ToString() : "";
			bool exists = false;
			if (old.hasKey(key)) {
				const json::JSON& oi = old[key];
				if (oi.hasKey(md5) && oi.at(md5) == hash) {
					exists = true;
				}
			}
			if (!exists) {
				std::filesystem::path heapfile = _path(hash);
				if (std::filesystem::exists(heapfile)) {
					std::filesystem::path p = _dest;
					p.append(key);
					try {
						zpp::reader zw(heapfile.string());
						zw.extractFirstToFile(p.string());
						if(md5::file_hash(p.string()) != hash) {
							errorHandler("Unable to replace the file <b>" + p.filename().string() + "</b>, probably program is run.");
							anyFail = true;
						} else {
							anyCopy = true;
						}
						if (item.hasKey("size")) {
							cur += item["size"].ToInt();
						}
						if (progressHandler(key, skipUserBreak ? "Undoing" : "Copying")) break;
					}
					catch (std::filesystem::filesystem_error& e) {
						errorHandler("InsufficientSpace", &e);
					}
				} else {
					errorHandler("DataAccessError");
				}
			}
		}
	}
	if (anyFail && anyCopy) {
		errorHandler("Unable to replace any file in the destination folder <b>" + _dest.string() + "</b>, probably because of the lack of ADMIN privileges.");
	}
	if (!err && remove_extra_files && _hashesList.length()) {
		stage = "Removing files";
		/// remove files not present in the image
		for (int i = 0; i < 2; i++) {
			for (auto [key, item] : old.ObjectRange()) {
				if (!image.hasKey(key)) {
					if (item.hasKey("md5") == (i == 0)) {						
						std::filesystem::path p = _dest;
						p.append(key);
						std::string m = "\n" + md5::file_hash(p.string()) + "\n";
						/// we delete only files that belong to the program's heap to prevent deleting user's files.
						/// _hashesList contains all hashes present in the heap - all files that was someday in the program's image.
						/// This prevents the potentially dangerous situation when the destination folder is incorrect and
						/// all files from that root are eliminated forever.
						if (m.length() == 34 && _hashesList.find(m) != std::string::npos) {
							try {
								std::filesystem::remove_all(p);
								if (log) std::cout << "Removed: " << p << "\n";
							}
							catch (std::filesystem::filesystem_error& e) {
								errorHandler("Unable to remove the file <b>" + p.filename().string() + "</b>, probably program is run.", &e);
								if (log) {
									std::cout << "Unable to remove: " << p << "\n";
									std::cout << e.what();
								}
							}
						}
					}
				}
			}
		}
	}
	if(userBreak) {
		/// restore original image
		syncDestination(old, remove_extra_files, true, exceptions);
	}
	return !err;
}

int fheap::FilesHeap::uploadHeap(const std::string& bucket_name) {
	if (!valid())return false;
	exec::CommandResult res;
	res.exitstatus = 1;
	if (_server.find("google") != std::string::npos) {
		std::cout << "uploading " << _heapPath.generic_string() << " => gs://" << bucket_name << "/heap" << _heapPath.generic_string() << "\n";
		std::string com = "gsutil -m rsync -d -r \"" + _heapPath.generic_string() + "\" \"gs://" + bucket_name + "/heap\"";
		res = exec::Command::exec(com);
		com = "gsutil setmeta -h \"cache-control:no-store\" \"gs://" + bucket_name + "/heap/Versions/root.json\"";
		exec::Command::exec(com);
		std::cout << res.output << "\n" << "Uploading finished.\n\n";
	}
	else if (_server.find("amazon") != std::string::npos) {
		std::cout << "uploading " << _heapPath.generic_string() << " => gs://" << bucket_name << "/heap" << _heapPath.generic_string() << "\n";
		std::string com = "aws s3 sync \"" + _heapPath.generic_string() + "\" s3://" + bucket_name + "/heap --acl=public-read";
		res = exec::Command::exec(com);
		std::cout << res.output << "\n" << "Uploading finished.\n\n";
	}
	else std::cout << "ERROR: Unsupported storage provider!\n";
	return res.exitstatus;
}

int fheap::FilesHeap::downloadHeap(const std::string& bucket_name) {
	if (!valid())return false;
	exec::CommandResult res;
	res.exitstatus = 1;
	if (_server.find("google") != std::string::npos) {
		/// google buckets
		std::cout << "downloading gs://" << bucket_name << "/heap => " << _heapPath.generic_string() << "\n";
		std::string com = "gsutil -m rsync -d -r \"gs://" + bucket_name + "/heap\" \"" + _heapPath.generic_string() + "\"";
		res = exec::Command::exec(com);
		std::cout << res.output << "\n" << "Downloading finished.\n\n";
	} else if (_server.find("amazon") != std::string::npos) {
		std::cout << "downloading s3://" << bucket_name << "/heap => " << _heapPath.generic_string() << "\n";
		std::string com = "aws s3 sync s3://" + bucket_name + "/heap \"" + _heapPath.generic_string() + "\"";
		res = exec::Command::exec(com);
		std::cout << res.output << "\n" << "Downloading finished.\n\n";
	}
	else std::cout << "ERROR: Unsupported storage provider!\n";
	return res.exitstatus;
}

bool fheap::FilesHeap::createDestFolderImage(json::JSON& image, bool addToHeap, bool useCache, const std::vector<std::string>* exceptions) {
	if (!valid())return false;
	json::JSON cache;
	std::filesystem::path cp = _heapPath;
	cp.append("cache.dat");
	if (useCache) {
		if (exists(cp)) {
			jcc::readSafeJson(cache, cp.string());	
		}
	}
	if (cache.IsNull())cache = json::Object();
	try {
		auto ftime = [](const std::filesystem::path& path)-> std::string {
			return std::to_string(
				std::chrono::duration_cast<std::chrono::seconds>(
					std::filesystem::last_write_time(path).time_since_epoch()).count());
		};
		size_t total = 0;
		size_t cur = 0;
		const std::string md5 = "md5";
		const std::string zip = "zip";
		std::vector<std::filesystem::directory_entry> files;
		for (auto& p : std::filesystem::recursive_directory_iterator(_dest)) {
			bool add = true;
			if(exceptions) {
				std::filesystem::path rel = std::filesystem::relative(p, _dest);
				for (int i = 0; i < exceptions->size(); i++) {
					if(jcc::wild_match(rel.string(), exceptions->at(i))) {
						add = false;
						break;
					}
				}
			}
			if (add) {
				files.push_back(p);
				total += 1000;
			}
		}
		std::map<std::string, bool> handled;
		std::map< std::string, size_t> countedAs;
		for (int i = 0; i < 2; i++) {
			handled.clear();
			int k = 0;
			for (auto& p : files) {
				size_t total0 = total;
				if(i)cur += 1000;
				if (progress && !progress(cur, total, p.path().string(), addToHeap ? "Updating the heap" : "Checking files")) {
					image = json::Object();
					return false;
				}
				if (p.is_regular_file()) {
					size_t filesize = file_size(p.path());
					bool already = false;

					// easiest check for file repeat - if size and name are same. But later check will be more careful - by md5
					// this is just to correct progress bar
					std::string s = std::to_string(filesize) + p.path().filename().string();
					if(handled.count(s))already = true;
					else handled[s] = true;
					
					std::string hash;
					std::string zhash;
					std::string time = ftime(p);
					std::string rel = relative(p, _dest).string();
					bool hashFromCache = false;
					if (useCache && cache.hasKey(rel)) {
						json::JSON& itm = cache[rel];
						if (itm.hasKey("time") && itm["time"].ToString() == time) {
							if (itm.hasKey(md5)) {
								hash = itm[md5].ToString();
								hashFromCache = true;
							}
							if (itm.hasKey(zip)) {
								zhash = itm[zip].ToString();
								if (zhash.length() != 32)zhash.clear();
							}
						}
					}
					json::JSON& itm = image[rel];
					if(itm.IsNull()) itm = json::Object();
					if (hash.size() != 32) {
						if (itm.hasKey(md5))hash = itm[md5].ToString();
						else {							
							size_t fsize = filesize / 10 + 1;;
							if (i == 0) {
								total += fsize;
							}else{
								hash = md5::file_hash(p.path().string());
							}
						}
					}
					if (hash.length() == 32 || i == 0) {
						std::filesystem::path zp = _path(hash);
						if (addToHeap) {
							bool exzp = exists(zp);
							if ((i == 0 && hash.empty()) || !exzp) {
								zhash.clear();
								if (i == 0) {
									if (!already) {
										total += filesize;
									}
								}
								else {									
									zpp::writer zw(zp.string());
									zw.addFile(p.path().string(), hash);
									zhash = md5::file_hash(zp.string());
								}								
							}							
							if (zhash.length() != 32 && (i == 0 || exists(zp))) {
								size_t fsize = filesize / 30 + 1;
								if (i == 0) total += fsize;								
								else zhash = md5::file_hash(zp.string());									
							}
						}
						if (zhash.length() == 32)itm[zip] = zhash;
						if (hash.length() == 32)itm[md5] = hash;
						itm["time"] = time;
						if (exists(zp))itm["size"] = std::to_string(std::filesystem::file_size(zp));
					}
				}
				else if (p.is_directory()) {
					std::string rel = relative(p, _dest).string();
					json::JSON& itm = image[rel] = json::Object();
					itm["folder"] = true;
					itm["time"] = ftime(p);
				}
				if(i == 0) {
					countedAs[p.path().string()] = total - total0;
				} else {
					cur += countedAs[p.path().string()];
					if (progress) {
						if (!progress(cur, total, p.path().string(), "Estimation")) {
							image = json::Object();
							return false;
						}
					}
				}
			}
			if (total == 0)break;
		}
	}
	catch (std::filesystem::filesystem_error& e) {
		if (log) std::cout << e.what();
	}
	if (useCache) {
		jcc::writeSafeJson(image, cp.string());
	}
	return true;
}

bool fheap::FilesHeap::valid() {
	return _heapPath.string().length() > 6 && _dest.string().length() > 6;
}

#include "ProjectsManager.h"

#include "exec.h"
#include "jcc.h"

gsproject::Manager::Manager() {
	SyncDown = true;
}

gsproject::Manager::Manager(const std::string& path) {
	std::cout << "Reading the config " << path << "\n";
	readConfig(path);
	std::cout << "done.\n";
}

void gsproject::Manager::readConfig(const std::string& path) {
	std::string conf = jcc::readFile(path);
	if (!conf.empty()) {
		json::JSON js = json::JSON::Load(conf);
		auto read = [&](const std::string& name, std::string& var) {
			if (js.hasKey(name)) {
				var = js[name].ToString();
			}
			else std::cout << "ERROR: The field " << name << " in the configuration file is mandatory";
		};
		read("PathToFiles", PathToFiles);
		read("HeapPath", HeapPath);
		read("VersionFileRelativePath", VersionFileRelativePath);
		read("Server", Server);
		read("RemoteFilesPath", RemoteFilesPath);
		read("Bucket", Bucket);
		if(js.hasKey("SyncDown")) {
			SyncDown = js["SyncDown"].ToBool();
		}
		if(js.hasKey("Exceptions")) {
			json::JSON& arr = js["Exceptions"];
			int n = arr.size();
			for (int i = 0; i < n; i++) {
				json::JSON& el = arr.at(i);
				std::string exc = el.ToString();
				if (exc.length())Exceptions.push_back(exc);
			}
		}
	}else {
		std::cout << "ERROR: Project configuration path incorrect!";
	}
}

template <typename TP>
std::time_t to_time_t(TP tp)
{
	using namespace std::chrono;
	auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
		+ system_clock::now());
	return system_clock::to_time_t(sctp);
}

bool gsproject::Manager::createImage(bool upload, const std::string versionFileName) {
	bool success = true;

	json::JSON override;
	std::filesystem::path ov = HeapPath;
	ov.append("Versions/stable.json");
	jcc::readSafeJson(override, ov.string());
	
	json::JSON this_version_json;
	/// read the version file from the source data
	std::filesystem::path version_json_path = PathToFiles;
	version_json_path.append(VersionFileRelativePath);
	std::string version_json_data = jcc::readFile(versionFileName.length() ? versionFileName : version_json_path.string());
	if (!version_json_data.empty()) {
		this_version_json = json::JSON::Load(version_json_data);
		auto check = [&](const std::string& name) {
			if (!this_version_json.hasKey(name)) {
				std::cout << "ERROR: The mandatory field is missing in the version file: " << name << "\n";
				success = false;
			}
		};
		check("Product");
		check("Version");
		check("Status");
		check("ReleaseDate");
		if (!success)return false;
	}
	else {
		std::cout << "ERROR: Version file missing at " << version_json_path << "\n";
		return false;
	}

	auto status = [&](const std::string& prod, const std::string& version, const std::string& origState) -> std::string {
		if (override.size()) {
			for (int i = 0; i < override.size(); i++) {
				json::JSON& el = override[i];
				if (el.hasKey("Product") && el.hasKey("Version") && el["Product"] == prod && el["Version"] == version) {
					return "Stable";
				}
			}
			return "Beta";
		}
		return origState;
	};

	this_version_json["Status"] = status(this_version_json["Product"].ToString(), this_version_json["Version"].ToString(), this_version_json["Status"].ToString());
	std::cout << "Creation of the image, product: " << this_version_json["Product"].ToString() << "\nversion: " << this_version_json["Version"].ToString() << "\nstatus: " << this_version_json["Status"].ToString() << "\n";
	
	auto replace_constants = [&](const std::string& s)->std::string {
		std::string r = jcc::_replace(s, "<<Version>>", this_version_json["Version"].ToString());
		r = jcc::_replace(r, "<<VERSION>>", this_version_json["Version"].ToString());
		if(jcc::wild_match(r, "*<<*>>*")) {
			std::cout << "Unknown replacement tag <<...>> in the path: " << r << "\n";
		}
		return r;
	};
	std::string path_to_files = replace_constants(PathToFiles);
	std::string release_date = this_version_json["ReleaseDate"].ToString();
	if (jcc::wild_match(release_date, "BY:*")) {
		std::cout << "Date should be defined by: " << release_date << "\n";
		release_date.erase(0, 3);
		std::string def_file = release_date;
		std::filesystem::path pd = path_to_files;
		pd.append(release_date);
		std::filesystem::directory_entry dd(pd);
		if(dd.exists()) {
			std::time_t tt = to_time_t(dd.last_write_time());
			std::ostringstream oss;
			oss << std::ctime(&tt);
			release_date = jcc::trim(oss.str());
			this_version_json["ReleaseDate"] = release_date;
			std::cout << "Release date defined by the " << def_file << " : " << release_date << "\n\n";
		}else {
			std::cout << "Date definition file not found: " << dd << "\n";
		}
		
	}
	heap.setServer(Server, "");
	heap.setDestinationFolder(path_to_files);
	heap.setHeapPlacement(replace_constants(HeapPath));
	std::cout << "\n" << "Data source folder: " << path_to_files << "\n";

	if (upload && !Bucket.empty() && SyncDown) {
		if(heap.downloadHeap(Bucket)) {
			std::cout << "ERROR: State downloading failed.";
			return false;
		}
	}

	jcc::writeSafeJson(override, ov.string());
	
	heap.createDestFolderImage(image, true, true, &Exceptions);
	std::cout << "\n";

	json::JSON versions_list_json;
	jcc::readSafeJson(versions_list_json, HeapPath + "Versions/root.json");
	if (versions_list_json.IsNull()) {
		versions_list_json = json::Array();
	}
	std::string relative_path_to_this_version = "Versions/" + this_version_json["Product"].ToString() + this_version_json["Version"].ToString();
	this_version_json["ImageURL"] = Server + RemoteFilesPath + "/heap/" + relative_path_to_this_version;
	bool this_version_present = false;
	for(int i=0;i<versions_list_json.size();i++) {
		json::JSON& js = versions_list_json[i];
		if(js.hasKey("Version") && js["Version"].ToString() == this_version_json["Version"].ToString() && js.hasKey("Product") && js["Product"].ToString() == this_version_json["Product"].ToString()) {
			js = this_version_json;
			this_version_present = true;
		}
		if (js.hasKey("Version") && js.hasKey("Product") && js.hasKey("Status")) {
			js["Status"] = status(js["Product"].ToString(), js["Version"].ToString(), js["Status"].ToString());
		}
	}
	if(!this_version_present)versions_list_json.append(this_version_json);
	std::filesystem::path version = HeapPath;
	version.append(relative_path_to_this_version + ".json");
	std::filesystem::path zipped_path = HeapPath;
	zipped_path.append(relative_path_to_this_version);
	std::filesystem::path versions_list_full_path = HeapPath;
	versions_list_full_path.append("Versions/root.json");
	jcc::writeSafeJson(versions_list_json, versions_list_full_path.string());
	jcc::writeSafeJson(image, version.string());
	zpp::writer w(zipped_path.string());
	w.addFile(version.string(), relative_path_to_this_version + ".json");
	w.flush();
	std::filesystem::remove(HeapPath + relative_path_to_this_version + ".json");

	std::vector<std::filesystem::directory_entry> files;
	std::filesystem::path hp = HeapPath;
	hp.append("Versions/heap.txt");
	std::ofstream f(hp, std::ios::binary);
	if (f.is_open()) {
		f << "\n";
		for (auto& p : std::filesystem::recursive_directory_iterator(HeapPath)) {
			if (p.is_regular_file()) {
				std::filesystem::path path = p.path();
				std::string fname = path.filename().string();
				if (path.extension().empty() && fname.length() == 32) {
					f << fname << "\n";
				}
			}
		}
		f.close();
		std::filesystem::path zp = HeapPath;
		zp.append("Versions/heap.dat");
		zpp::writer z(zp.string());
		z.addFile(hp.string(), "heap.txt");
		z.flush();
		remove(hp);
	}
	
	if (upload && !Bucket.empty()) {
		if(heap.uploadHeap(Bucket)) {
			std::cout << "ERROR: State uploading failed.";
			return false;
		}
	}
	return true;
}

std::string gsproject::Manager::gsutil(const std::string& params) {
	std::string com = "gsutil " + params;
	exec::CommandResult res = exec::Command::exec(com);
	return res.output;
}

#pragma once
#include <codecvt>
#include <functional>
#include <filesystem>
#include "json.h"
#include "md5.h"

#ifdef _WIN32
#include <shellapi.h>
#endif
namespace httplib {
	class Server;
};

namespace jcc {
	class Html;

	class Request:public httplib::Request {
	public:
		///tries to convert params to json, if empty -  converts body to JSON
		operator json::JSON() const;
		///explicitly converts params of the request to JSON
		json::JSON paramsToJson() const;
		///explicitly converts the body of the request to JSON
		json::JSON bodyToJson() const;
	};
	class Response:public httplib::Response {
	public:
		///Assign the json directly as the request response
		void operator = (const json::JSON& js);
		///Assign the HTML as the request response;
		void operator = (const Html& h);
		///Assign text as the response result
		void operator = (const std::string& string);
		///Assign text as the response result
		void operator = (const char* string);
	};
	typedef std::function<void(const Request&, Response&)> requestHandler;
	class LocalServer;
	
	class Html {
	public:
		std::string _body;
		Html();

		/// read file by the absolute path
		Html(const char* filepath);
		
		///read the html from the server's files, path should be relative to server
		Html(const char* filepath, LocalServer& server);
		
		operator const char* ();
		operator std::string& ();

		///replace all, for the simplicity of the substitution
		void Replace(const std::string& ID, const std::string& text);
		
		template <class T>
		void operator << (T value) {
			std::stringstream ss;
			ss << value;
			_body += ss.str();
		}

		/// making html using the chain operators
		Html& tag(const char* tagName, const char* attributes, const Html& inner);
	};

	class LocalServer {
		httplib::Server* svr;
		std::function<json::JSON(json::JSON&)> _exchange;
		int _port;
		Html home;
		bool _allowConsoleOutput;
		bool _autoTerminate;
		bool _stop;
		bool _pinged;
		std::chrono::time_point<std::chrono::system_clock> ping_time;
		std::mutex ping_mutex;
		std::string dump_headers(const httplib::Headers& headers);
		std::string log(const httplib::Request& req, const httplib::Response& res);
		static std::string& _server();
		static void _define_server();	
	public:
		LocalServer(int preferred_port = -1);

		/// return the reference to the httplib::Server for any advanced operations.
		httplib::Server& server() {
			return *svr;
		}

		/// Set the path to the folder that contains /public folder
		/// The main.js should be placed into the root of that folder.
		/// If you will not specify the path, the folder that contains main.js will be taken as the root folder
		/// will be treated as the root folder for the server.
		static void setServerFilesPlacement(const char* path);

		/// returns the port, assigned to the server
		int port();

		/// allows logging about every network action to the to cout
		void allowConsoleOutput(bool enable = true);

		/// opens the page in browser
		void open(const Html& page);
		
		/// opens the page from file in browser
		void openFile(const std::string& filepath);

		/// assign the callback for the get request. The page may interact with the host program via the get requests.
		void get(requestHandler f, const char* pattern = nullptr);

		/// assign the callback for the post request. The page may interact with the host program via the post requests.
		void post(requestHandler f, const char* pattern = nullptr);

		/// assign the callback for the put request.
		void put(requestHandler f, const char* pattern = nullptr);

		///exchange objects, you are getting object as input and should return the object as well. The call of exchange initiated on browser's side by sendObject, see the example
		void exchange(std::function<json::JSON(const json::JSON&)> f);

		/// run once, it blocks execution and starts to listen the requests. Create the thread if no need blocking.
		void listen();

		/// stop the server and exit the listen cycle. This function may be called in the response body. Server will be stopped only after the sending the response.
		void signalToStop();

		///stop and wait till it will be stopped
		void stopGracefully();

		/// Wait till the page will be closed or the connection lost.
		/// \param ms_amout the milliseconds to wait, -1 to wait infinitely.
		/// \returns true if the connection lost.
		bool wait(int ms_amount = -1);

		/// returns milliseconds since the last ping or < 0 if there was no pings.
		double msSincePing();

		/// appends filename to the server files path, returns the absolute path
		static std::string path(const std::string& filename);
		
		/// read the file to string, path is relative to server
		static void read(const std::string& filename, std::string& to);
		
		/// write the string to file, path is relative to server
		static void write(const std::string& filename, std::string& from);
		
		/// create all componentes of the path, path is relative to server
		static void createPath(const std::string& path);
		
		/// detects if the path is relative to the server
		static bool pathIsRelative(const std::string& path);
	};

	/// Useful utility stuff:

	/// returns the path to the current executable 
	std::filesystem::path getexepath();
	
	/// matches wildcards
	inline bool wild_match(const std::string& str, const std::string& pat);

	/// reads the file into string and returns the string
	std::string readFile(const std::string& fpath);

	//creates the path for the file or folder
	void createPath(const std::string& destFilename);

	// saves the string to file
	std::string writeFile(const std::string& fpath, const std::string& string);

	/// read json signed at the beginning with it's md5 to ensure it is not corrupted or changed manually
	bool readSafeJsonFromString(json::JSON& dest, const std::string& string);

	/// read json signed at the beginning with it's md5 to ensure it is not corrupted or changed manually
	bool readSafeJson(json::JSON& dest, const std::string& path);

	/// write json signed at the beginning with it's md5 to ensure it is not corrupted or changed manually
	void writeSafeJson(const json::JSON& json, const std::string& path);

	/// read json from the file
	void readJson(json::JSON& dest, const std::string& path);

	/// write json to the file
	void writeJson(const json::JSON& json, const std::string& path);

	/// return command line arguments as array, in case of console app you need to call the passArgs() in the beginning of the main(...)
	inline std::vector<std::string>& args();

	/// call it at the beginning of the main(...) to be able to use the previous function
	void passArgs(int argc, char* argv[]);

	/// get the value of the command line argument, passed as /value "the value itself" - in this case pass the "/value" and get "the value itself"
	std::string getArg(const std::string& argname);

	/// remove any slashes at the end of the string
	void TrimSlash(std::string& s);

	/// remove the filename from the string
	void RemoveFileName(std::string& s);
	

	/** \brief Perform the fetch get request.
	* \param server the server
	* \param request the get request command
	* \param error the error callbacká øà
	* \param progress the progress callback, return false to cancel
	*/
	inline std::string fetchGet(const std::string& server, const std::string& request,
		std::function<void(const httplib::Result&)> error = nullptr,
		std::function<bool(const char* data, size_t data_length)> getter = nullptr);
	

	inline std::string readFile(const std::string& fpath) {
		std::string to;
		std::ifstream  fs(fpath, std::fstream::in | std::fstream::binary);
		if (fs.is_open()) {
			std::stringstream buffer;
			buffer << fs.rdbuf();
			to = buffer.str();
			fs.close();
		}
		return to;
	}

	inline void createPath(const std::string& destFilename) {
		try {
			std::filesystem::path f = destFilename;
			if (f.has_filename()) f.remove_filename();
			std::filesystem::create_directories(f);
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
		}
	}

	inline std::string writeFile(const std::string& fpath, const std::string& string) {
		createPath(fpath);
		std::string to;
		std::ofstream  fs(fpath, std::fstream::out | std::fstream::binary);
		if (fs.is_open()) {
			fs << string;
			fs.close();
		}
		return to;
	}

	inline std::string fetchGet(const std::string& server, const std::string& request, 
		std::function<void(const httplib::Result&)> error,
		std::function<bool(const char* data, size_t data_length)> getter) {

		httplib::Client cl(server);
		std::string cum;
		auto res = cl.Get(request.c_str(),
			[&](const char* data, size_t data_length) {
				cum.append(data, data_length);
				if (getter)return getter(data, data_length);
				return true;
			});
		if (((res && res->status != 200) || !res) && error) {
			error(res);
			cum = "";
		}
		return cum;
	}
	
	inline bool wild_match(const std::string& str, const std::string& pat) {
		auto deslash = [](char c) -> char { return c == '\\' || c == '/' ? '/' : c; };
		std::string::const_iterator str_it = str.begin();
		for (std::string::const_iterator pat_it = pat.begin(); pat_it != pat.end();
			++pat_it) {
			switch (*pat_it) {
			case '?':
				if (str_it == str.end()) {
					return false;
				}

				++str_it;
				break;
			case '*': {
				if (pat_it + 1 == pat.end()) {
					return true;
				}

				const size_t max = strlen(&*str_it) - 1;
				for (size_t i = 0; i < max; ++i) {
					if (wild_match(&*(str_it + i), &*(pat_it + 1))) {
						return true;
					}
				}

				return false;
			}
			default:
			{
				if (str_it == str.end() && pat_it != pat.end())return false;
				char c1 = *str_it;
				char c2 = *pat_it;
				if (deslash(c1) != deslash(c2)) {
					return false;
				}
				++str_it;
			}
			}
		}
		return str_it == str.end();
	}

	inline bool readSafeJsonFromString(json::JSON& dest, const std::string& string) {
		std::string res = string;
		if (res.length() > 34) {
			if (res[0] == '#' && res[33] == '\n') {
				std::string md5 = res.substr(1, 32);
				res.erase(0, 34);
				if (md5::hash(res) == md5) {
					if(res != "null")dest = json::JSON::Load(res);
					return true;
				}
			}
		}
		return false;
	}


	inline bool readSafeJson(json::JSON& dest, const std::string& path) {
		std::string res = readFile(path);
		return readSafeJsonFromString(dest, res);
	}

	inline void writeSafeJson(const json::JSON& json, const std::string& path) {
		std::string res = json.dump();
		writeFile(path, "#" + md5::hash(res) + "\n" + res);
	}

	inline void readJson(json::JSON& dest, const std::string& path) {
		std::string res = readFile(path);
		dest = json::JSON::Load(res);
	}

	/// write json to the file
	inline void writeJson(const json::JSON& json, const std::string& path) {
		std::string res = json.dump();
		writeFile(path, res);
	}

	inline std::vector<std::string>& args() {
		static std::vector<std::string>* keeper = nullptr;
		if (!keeper) {
			keeper = new std::vector<std::string>;
#ifdef _WIN32
			int nArgs = 0;
			bool ins = false;
			std::string args = GetCommandLine();
			for (auto a : args) {
				if (a == '\"') {
					if (ins)ins = false;
					else {
						keeper->push_back("");
						ins = true;
					}
				}else
				if (a != ' ' && a != '\t') {
					if (!ins) {
						keeper->push_back("");
						ins = true;
					}
					keeper->back() += a;
				}
				else {
					ins = false;
				}
			}
		}
#endif
		return *keeper;
	}

	inline void passArgs(int argc, char* argv[]) {
		std::vector<std::string>& a = args();
		a.clear();
		for (int i = 0; i < argc; i++) {
			a.push_back(argv[i]);
		}
	}

	inline std::string getArg(const std::string& argname) {
		std::vector<std::string>& a = args();
		for (int i = 0; i < a.size(); i++) {
			if (a[i] == argname && i < a.size() - 1) {
				return a[i + 1];
			}
		}
		return "";
	}

	inline std::string LocalServer::dump_headers(const httplib::Headers& headers) {
		std::string s;
		char buf[BUFSIZ];

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			const auto& x = *it;
			snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
			s += buf;
		}

		return s;
	}

	inline std::string LocalServer::log(const httplib::Request& req, const httplib::Response& res) {
		std::string s;
		if (_allowConsoleOutput) {
			char buf[BUFSIZ];

			s += "================================\n";

			snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
				req.version.c_str(), req.path.c_str());
			s += buf;

			std::string query;
			for (auto it = req.params.begin(); it != req.params.end(); ++it) {
				const auto& x = *it;
				snprintf(buf, sizeof(buf), "%c%s=%s",
					(it == req.params.begin()) ? '?' : '&', x.first.c_str(),
					x.second.c_str());
				query += buf;
			}
			snprintf(buf, sizeof(buf), "%s\n", query.c_str());
			s += buf;

			s += dump_headers(req.headers);

			s += "--------------------------------\n";

			snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
			s += buf;
			s += dump_headers(res.headers);
			s += "\n";

			if (!res.body.empty()) { s += res.body; }

			s += "\n";
		}

		return s;
	}

	inline void EnsureTrailingSlash(std::string& s) {
		size_t L = s.length();
		if (L && s[L - 1] != '\\' && s[L - 1] != '/')s += "/";
	}

	inline void TrimSlash(std::string& s) {
		do {
			size_t L = s.length();
			if (L && (s[L - 1] == '\\' || s[L - 1] == '/'))s.pop_back();
			else break;
		} while (s.length());
	}

	inline void RemoveFileName(std::string& s) {
		do {
			size_t L = s.length();
			if (L && s[L - 1] != '\\' && s[L - 1] != '/')s.pop_back();
			else break;
		} while (s.length());
	}

	inline Request::operator json::JSON() const {
		if (params.size()) {
			return paramsToJson();
		}else {
			return bodyToJson();
		}
	}

	inline json::JSON Request::paramsToJson() const {
		json::JSON js;
		for (auto it = params.begin(); it != params.end(); it++) {
			js[it->first] = it->second;
		}
		return js;
	}

	inline json::JSON Request::bodyToJson() const {
		json::JSON js = json::JSON::Load(body.c_str());
		return js;
	}

	inline void Response::operator=(const json::JSON& js) {
		set_content(js.dump(), "application/json");
	}

	inline void Response::operator=(const Html& h) {
		set_content(h._body.c_str(), "text/html");
	}

	inline void Response::operator=(const std::string& str) {
		set_content(str.c_str(), "text/html");
	}

	inline void Response::operator=(const char* str) {
		set_content(str, "text/html");
	}

	inline std::filesystem::path getexepath() {
#ifdef _WIN32
		wchar_t path[MAX_PATH] = { 0 };
		GetModuleFileNameW(NULL, path, MAX_PATH);
		return path;
#else
		char result[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
		return std::string(result, (count > 0) ? count : 0);
#endif
	}
	
	inline std::string& LocalServer::_server() {
		static std::string s;
		return s;
	}

	inline void LocalServer::_define_server() {
		if (_server().length() == 0) {
			_server() = getexepath().string();
			EnsureTrailingSlash(_server());
			do {
				std::string fp = _server();
				EnsureTrailingSlash(fp);
				fp += "/main.js";
				if (std::filesystem::exists(std::filesystem::path(fp.c_str()))) {
					std::cout << "main.js found at: " << fp << "\n";
					break;
				}
				else {
					TrimSlash(_server());
					RemoveFileName(_server());
					EnsureTrailingSlash(_server());
				}
			} while (_server().length() > 3);
		}
		std::cout << "Server files location: " << _server() << "\n";
	}

	inline std::string LocalServer::path(const std::string& filename) {
		_define_server();
		if (pathIsRelative(filename)) {
			std::string fp = _server();
			fp += filename;
			return fp;
		}
		else return std::string(filename);
	}

	inline void LocalServer::setServerFilesPlacement(const char* path) {
		_server() = path;
		EnsureTrailingSlash(_server());
	}

	inline void LocalServer::read(const std::string& filename, std::string& to) {
		_define_server();
		std::string fp = _server() + filename;
		std::ifstream  fs(fp, std::fstream::in | std::fstream::binary);
		if (fs.is_open()) {
			std::stringstream buffer;
			buffer << fs.rdbuf();
			to = buffer.str();
			fs.close();
		}
	}

	inline void LocalServer::write(const std::string& filename, std::string& from) {
		_define_server();
		std::string fp = _server() + filename;
		jcc::createPath(fp);
		std::fstream fs(fp, std::fstream::out | std::fstream::binary);
		if(fs.is_open()){
			fs << from;
			fs.close();
		}
	}

	inline void LocalServer::createPath(const std::string& path) {
		_define_server();
		std::filesystem::path p = path;
		p.remove_filename();
		std::filesystem::create_directories(p);
	}

	inline bool LocalServer::pathIsRelative(const std::string& path) {
		if (!path.empty()) {
			if (path[0] == '/')return false;
			if (path.length() > 1 && path[1] == ':')return false;
		}
		return true;
	}

	inline Html::Html() {

	}

	inline Html::Html(const char* filepath) {
		_body = jcc::readFile(filepath);
	}

	inline Html::Html(const char* filepath, LocalServer& server) {
		_body = jcc::readFile(server.path(filepath));
		std::cout << "HTML read: " << filepath << " Lenght: " << _body.length() << " \n";
	}

	inline Html::operator const char* () {
		return _body.c_str();
	}

	inline Html::operator std::string& () {
		return _body;
	}

	// trim from end of string (right)
	inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
	{
		s.erase(s.find_last_not_of(t) + 1);
		return s;
	}

	// trim from beginning of string (left)
	inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
	{
		s.erase(0, s.find_first_not_of(t));
		return s;
	}

	// trim from both ends of string (right then left)
	inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
	{
		return ltrim(rtrim(s, t), t);
	}
	
	inline std::string _replace(const std::string& str, const std::string& sub, const std::string& mod) {
		std::string tmp(str);
		auto fnd = tmp.find(sub);
		if (fnd != -1)tmp.replace(fnd, sub.length(), mod);
		return tmp;
	}

	inline void Html::Replace(const std::string& ID, const std::string& text) {
		_body = _replace(_body, ID, text);
	}

	inline Html& Html::tag(const char* tagName, const char* attributes, const Html& inner) {
		_body += "<" + std::string(tagName) + (attributes && attributes[0] ? " " + std::string(attributes) : "") + ">" + inner._body + "</" + tagName + ">";
		return *this;
	}

	inline LocalServer::LocalServer(int preferred_port) {
		_allowConsoleOutput = false;
		_autoTerminate = true;
		_stop = false;
		_pinged = false;
		svr = new httplib::Server;
		if (!svr->is_valid()) {
			printf("server has an error...\n");
			return;
		}
		svr->Get("/ping", [=](const httplib::Request& req, httplib::Response& res) {
			ping_mutex.lock();
			_pinged = true;
			ping_time = std::chrono::system_clock::now();
			ping_mutex.unlock();
			res.set_content("alive", "text/html");
			});
		svr->Get("/home", [=](const httplib::Request& req, httplib::Response& res) {
			std::string h = home;
			res.set_content(h, "text/html");
			});
		std::string fpath = path("public");
		svr->set_mount_point("/", fpath);
		svr->set_error_handler([](const httplib::Request& req, httplib::Response& res) {
			const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
			char buf[BUFSIZ];
			snprintf(buf, sizeof(buf), fmt, res.status);
			res.set_content(buf, "text/html");
			});
		svr->set_logger([this](const httplib::Request& req, const httplib::Response& res) {
			printf("%s", log(req, res).c_str());
			});
		svr->set_file_request_handler([this](const httplib::Request& req, httplib::Response& res) {
			printf("File request: %s", log(req, res).c_str());
			});
		svr->set_post_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
			if(_stop) {
				svr->stop();
			}
			});
		_port = preferred_port;
		if (preferred_port == -1)_port = svr->bind_to_any_port("localhost");
		else {
			if (!svr->bind_to_port("localhost", preferred_port)) {
				_port = svr->bind_to_any_port("localhost");
			}
		}
	}

	inline int LocalServer::port() {
		return _port;
	}

	inline void LocalServer::allowConsoleOutput(bool enable) {
		_allowConsoleOutput = enable;
	}

	inline void LocalServer::open(const Html& page) {
		home = page;
		std::string lib = readFile(path("main.js"));
		if (strstr(home, "<body>")) {
			std::string blib = "<body>\n<script>\n" + lib + "\n</script>";
			home.Replace("<body>", blib.c_str());
		}
		std::string h = "start http://localhost:" + std::to_string(_port) + "/home";
		std::system(h.c_str());
	}

	inline void LocalServer::openFile(const std::string& filepath) {
		Html p;
		p._body = readFile(path(filepath));
		open(p);
	}

	inline void LocalServer::get(requestHandler f, const char* pattern) {
		if(svr)svr->Get(pattern ? pattern : "(.*?)", [=](const httplib::Request& req, httplib::Response& res) {
			const Request* _request = static_cast<const Request*>(&req);
			Response* _result = static_cast<Response*>(&res);
			f(*_request, *_result);
			res.status = 200;
		});
	}

	inline void LocalServer::post(requestHandler f, const char* pattern) {
		if (svr)svr->Post(pattern ? pattern : "(.*?)", [=](const httplib::Request& req, httplib::Response& res) {
			const Request* _request = static_cast<const Request*>(&req);
			Response* _result = static_cast<Response*>(&res);
			f(*_request, *_result);
			res.status = 200;
		});
	}

	inline void LocalServer::put(requestHandler f, const char* pattern) {
		if (svr)svr->Put(pattern ? pattern : "(.*?)", [=](const httplib::Request& req, httplib::Response& res) {
			const Request* _request = static_cast<const Request*>(&req);
			Response* _result = static_cast<Response*>(&res);
			f(*_request, *_result);
			res.status = 200;
			});
	}

	inline void LocalServer::exchange(std::function<json::JSON(const json::JSON&)> f) {
		if (svr)svr->Post("/exchange", [=](const httplib::Request& req, httplib::Response& res) {
			json::JSON js = json::JSON::Load(req.body);
			std::string s1 = js.dump();
			json::JSON r = f(js);
			res.set_content(r.dump(), "application/json");
			res.status = 200;
		});
	}

	inline void LocalServer::listen() {
		if(svr)svr->listen_after_bind();
	}

	inline void LocalServer::signalToStop() {
		if(svr) {
			if(_stop) {
				svr->stop();
			}
			_stop = true;
		}
	}

	inline void LocalServer::stopGracefully() {
		_stop = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		if (svr->is_valid()) {
			svr->stop();
		}
	}
	inline bool LocalServer::wait(int ms_amount)	{
		do {
			if (ms_amount)std::this_thread::sleep_for(std::chrono::milliseconds(ms_amount > 0 ? ms_amount : 10));
#ifdef _DEBUG
			if (msSincePing() > 200000)return true;
#else
			if (msSincePing() > 2000)return true;
#endif
			if (ms_amount >= 0) break;
		} while (true);
		return false;
	}
	inline double LocalServer::msSincePing() {
		if (_pinged) {
			ping_mutex.lock();
			std::chrono::duration<double> d = std::chrono::system_clock::now() - ping_time;
			ping_mutex.unlock();
			return d.count() * 1000.0;
		}
		return -1.0;
	}
};

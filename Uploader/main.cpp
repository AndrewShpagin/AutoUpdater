#include "../Common/HeapFilesSync.h"
#include "../Common/jcc.h"
#include "../Common/ProjectsManager.h"

const char* proj_example = "{\n"\
"    'PathToFiles' : 'the_path_to_files_to_be_deployed_on_user_side',\n"\
"    'HeapPath' : 'the_path_to_the temporary_heap_folder',\n"\
"    'Exceptions' : [\n"\
"        '.git*'\n"\
"        '.gitignore*'\n"\
"    ],\n"\
"    'VersionFileRelativePath': 'relative_path/Version.json',\n"\
"    'Server' : 'https://storage.googleapis.com',\n"\
"    'RemoteFilesPath' : '/your_bucket_name',\n"\
"    'Bucket' : 'yout_bucket_name',\n"\
"    'VersionsList' : 'https://storage.googleapis.com/test_install/Versions/root.json',\n"\
"}\n";

const char* vers_example = "{\n"\
"    'Product': '3DCoat',\n"\
"    'Version' : '2021.39',\n"\
"    'Status' : 'Stable',\n"\
"    'Publisher' : 'Pilgway',\n"\
"    'FeaturesURL' : 'https://pilgway.com/files/3dcoat/2021/updates.html',\n"\
"    'ReleaseDate' : 'Sun Aug 29 20:59:48 2021'\n"\
"}\n";

std::string quote(const char* s) {
	std::string r = s;
	for (auto c = r.begin(); c != r.end(); ++c) {
		if (*c == '\'')*c = '"';
	}
	return r;
}
int main(int na, char* argv[])
{
	std::string projPath;
	std::string version;
	bool build = false;
	bool upload = false;
	for (size_t i = 0; i < na; i++) {
		std::string arg = argv[i];
		if (arg == "/proj") {
			if (i < na - 1) {
				projPath = argv[i + 1];
			}
		}
		if (arg == "/build") {
			build = true;
		}
		if (arg == "/upload") {
			upload = true;
		}
		if (arg == "/version") {
			if (i < na - 1) {
				version = argv[i + 1];
			}
		}
	}
	if (build && projPath.length()) {
		gsproject::Manager m(projPath);
		return m.createImage(upload, version) ? 0 : 1;
	} else {
		std::cout << "CLI arguments:\n";
		std::cout << "/build - build the image.\n";
		std::cout << "/proj \"path_to_project.json\" - path to project.\n";
		std::cout << "/version \"path_to_version_description.json\" - optional parameter, take the version info from this path instead of taking from the build data.\n";
		std::cout << "/upload - upload the changes to the bucket. gsutil should be installed, you should be authorized to upload data to the bucket using the \"gcloud auth login\".\n";
		std::cout << "          See the \"https://cloud.google.com/sdk/docs/downloads-interactive\"";
		std::cout << "\n\nThe project file structure:\n";
		std::cout << quote(proj_example);
		std::cout << "\n\nThe version file example:\n";
		std::cout << quote(vers_example);
		
	}
	return 0;
}
